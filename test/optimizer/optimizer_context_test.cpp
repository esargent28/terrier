#include <memory>
#include <stack>
#include <utility>
#include <vector>

#include "optimizer/binding.h"
#include "optimizer/optimizer_context.h"
#include "optimizer/optimizer_defs.h"
#include "optimizer/optimizer_task.h"
#include "optimizer/optimizer_task_pool.h"
#include "optimizer/pattern.h"

#include "test_util/test_harness.h"

namespace terrier::optimizer {

struct OptimizerContextTest : public TerrierTest {
  void SetUp() override { TerrierTest::SetUp(); }

  void TearDown() override { TerrierTest::TearDown(); }
};

// NOLINTNEXTLINE
TEST_F(OptimizerContextTest, PatternTest) {
  // Creates a Pattern and makes sure everything is set correctly
  auto join = new Pattern(OpType::LOGICALINNERJOIN);
  auto left_child = new Pattern(OpType::LOGICALGET);
  auto right_child = new Pattern(OpType::LOGICALEXTERNALFILEGET);
  join->AddChild(left_child);
  join->AddChild(right_child);

  // Pattern should own its children
  EXPECT_EQ(join->GetChildPatternsSize(), 2);
  EXPECT_EQ(join->Type(), OpType::LOGICALINNERJOIN);

  EXPECT_EQ(join->Children().size(), 2);
  EXPECT_EQ(join->Children()[0]->GetChildPatternsSize(), 0);
  EXPECT_EQ(join->Children()[1]->GetChildPatternsSize(), 0);

  EXPECT_EQ(join->Children()[0]->Type(), OpType::LOGICALGET);
  EXPECT_EQ(join->Children()[1]->Type(), OpType::LOGICALEXTERNALFILEGET);
  delete join;
}

// NOLINTNEXTLINE
TEST_F(OptimizerContextTest, OptimizerTaskStackTest) {
  auto task_stack = new OptimizerTaskStack();

  std::stack<OptimizerTask *> track_stack;
  for (size_t i = 0; i < 5; i++) {
    auto task = new OptimizeGroup(nullptr, nullptr);
    track_stack.push(task);
    task_stack->Push(task);
  }

  EXPECT_TRUE(!task_stack->Empty());
  while (!task_stack->Empty()) {
    auto *task_stack_pop = task_stack->Pop();
    auto *track_pop = track_stack.top();
    track_stack.pop();
    EXPECT_EQ(task_stack_pop, track_pop);

    delete task_stack_pop;
  }

  EXPECT_TRUE(track_stack.empty());
  delete task_stack;
}

// NOLINTNEXTLINE
TEST_F(OptimizerContextTest, OptimizerTaskStackRemainTest) {
  auto task_stack = new OptimizerTaskStack();

  for (size_t i = 0; i < 5; i++) {
    auto task = new OptimizeGroup(nullptr, nullptr);
    task_stack->Push(task);
  }

  // Shouldn't leak memory!
  EXPECT_TRUE(!task_stack->Empty());
  delete task_stack;
}

// NOLINTNEXTLINE
TEST_F(OptimizerContextTest, OptimizerContextTaskStackTest) {
  auto context = OptimizerContext(nullptr);

  auto task_stack = new OptimizerTaskStack();
  task_stack->Push(new OptimizeGroup(nullptr, nullptr));
  context.SetTaskPool(task_stack);

  auto *pushed = new OptimizeGroup(nullptr, nullptr);
  context.PushTask(pushed);
  EXPECT_EQ(task_stack->Pop(), pushed);
  EXPECT_TRUE(!task_stack->Empty());
  delete pushed;
}

// NOLINTNEXTLINE
TEST_F(OptimizerContextTest, OptimizerContextTaskStackNullptrTest) {
  auto context = OptimizerContext(nullptr);
  context.SetTaskPool(nullptr);

  auto task_stack = new OptimizerTaskStack();
  task_stack->Push(new OptimizeGroup(nullptr, nullptr));
  context.SetTaskPool(task_stack);

  // This should clean up memory
  context.SetTaskPool(nullptr);
}

// NOLINTNEXTLINE
TEST_F(OptimizerContextTest, RecordOperatorNodeIntoGroupDuplicateSingleLayer) {
  auto context = OptimizerContext(nullptr);

  // Create OperatorNode of JOIN <= (GET A, GET A)
  std::vector<std::unique_ptr<AbstractOptimizerNode>> c;
  std::unique_ptr<OperatorNode> left_get = std::make_unique<OperatorNode>(
      LogicalGet::Make(catalog::db_oid_t(1), catalog::namespace_oid_t(2), catalog::table_oid_t(3), {}, "tbl", false),
      std::move(c));

  std::unique_ptr<OperatorNode> right_get =
      std::unique_ptr<OperatorNode>(dynamic_cast<OperatorNode *>(left_get->Copy().release()));
  EXPECT_EQ(*left_get, *right_get);
  auto rg_copy = right_get->Copy();
  auto lg_copy = left_get->Copy();

  std::vector<std::unique_ptr<AbstractOptimizerNode>> jc;
  jc.emplace_back(std::move(left_get));
  jc.emplace_back(std::move(right_get));
  auto *test_opnode = new OperatorNode(LogicalInnerJoin::Make(), std::move(jc));

  std::unique_ptr<OperatorNode> join_opnode = std::unique_ptr<OperatorNode>(test_opnode);
  auto join = std::unique_ptr<AbstractOptimizerNode>(join_opnode.release());

  // RecordOptimizerNodeIntoGroup
  GroupExpression *join_gexpr;
  EXPECT_TRUE(context.RecordOptimizerNodeIntoGroup(common::ManagedPointer(join.get()), &join_gexpr));
  EXPECT_TRUE(join_gexpr != nullptr);

  EXPECT_EQ(join_gexpr->Contents(), join->Contents());
  EXPECT_EQ(join_gexpr->GetChildGroupIDs().size(), 2);
  EXPECT_EQ(join_gexpr->GetChildGroupId(0), join_gexpr->GetChildGroupId(1));

  auto child = join_gexpr->GetChildGroupId(0);
  auto group = context.GetMemo().GetGroupByID(child);
  EXPECT_EQ(group->GetLogicalExpressions().size(), 1);

  auto child_gexpr = group->GetLogicalExpressions()[0];
  EXPECT_EQ(child_gexpr->Contents(), lg_copy->Contents());
  EXPECT_EQ(child_gexpr->Contents(), rg_copy->Contents());
  EXPECT_EQ(child_gexpr->GetChildGroupIDs().size(), 0);
}

// NOLINTNEXTLINE
TEST_F(OptimizerContextTest, RecordOperatorNodeIntoGroupDuplicateMultiLayer) {
  auto context = OptimizerContext(nullptr);

  // Create OperatorNode (A JOIN B) JOIN (A JOIN B)
  std::vector<std::unique_ptr<AbstractOptimizerNode>> c;
  auto left_get = std::make_unique<OperatorNode>(
      LogicalGet::Make(catalog::db_oid_t(1), catalog::namespace_oid_t(2), catalog::table_oid_t(3), {}, "tbl", false),
      std::move(c));
  auto lg_copy = left_get->Copy();

  auto right_get = std::unique_ptr<OperatorNode>(dynamic_cast<OperatorNode *>(left_get->Copy().release()));
  auto rg_copy = right_get->Copy();
  EXPECT_EQ(*left_get, *right_get);

  std::vector<std::unique_ptr<AbstractOptimizerNode>> jc;
  jc.emplace_back(std::move(left_get));
  jc.emplace_back(std::move(right_get));
  auto left_join = std::make_unique<OperatorNode>(LogicalInnerJoin::Make(), std::move(jc));
  auto lj_copy = left_join->Copy();

  auto right_join = std::unique_ptr<OperatorNode>(dynamic_cast<OperatorNode *>(left_join->Copy().release()));
  auto rj_copy = right_join->Copy();
  EXPECT_EQ(*left_join, *right_join);

  std::vector<std::unique_ptr<AbstractOptimizerNode>> jjc;
  jjc.emplace_back(std::move(left_join));
  jjc.emplace_back(std::move(right_join));
  auto join = std::make_unique<OperatorNode>(LogicalInnerJoin::Make(), std::move(jjc))->Copy();

  // RecordOptimizerNodeIntoGroup
  GroupExpression *join_g_expr;
  EXPECT_TRUE(context.RecordOptimizerNodeIntoGroup(common::ManagedPointer(join), &join_g_expr));
  EXPECT_TRUE(join_g_expr != nullptr);

  EXPECT_EQ(join_g_expr->Contents(), join->Contents());
  EXPECT_EQ(join_g_expr->GetChildGroupIDs().size(), 2);
  EXPECT_EQ(join_g_expr->GetChildGroupId(0), join_g_expr->GetChildGroupId(1));

  auto join_child = join_g_expr->GetChildGroupId(0);
  auto join_group = context.GetMemo().GetGroupByID(join_child);
  EXPECT_EQ(join_group->GetLogicalExpressions().size(), 1);

  auto join_gexpr = join_group->GetLogicalExpressions()[0];
  EXPECT_EQ(join_gexpr->Contents(), lj_copy->Contents());
  EXPECT_EQ(join_gexpr->Contents(), rj_copy->Contents());
  EXPECT_EQ(join_gexpr->GetChildGroupIDs().size(), 2);
  EXPECT_EQ(join_gexpr->GetChildGroupId(0), join_gexpr->GetChildGroupId(1));

  auto child = join_gexpr->GetChildGroupId(0);
  auto child_group = context.GetMemo().GetGroupByID(child);
  EXPECT_EQ(child_group->GetLogicalExpressions().size(), 1);

  auto child_gexpr = child_group->GetLogicalExpressions()[0];
  EXPECT_EQ(child_gexpr->Contents(), lg_copy->Contents());
  EXPECT_EQ(child_gexpr->Contents(), rg_copy->Contents());
  EXPECT_EQ(child_gexpr->GetChildGroupIDs().size(), 0);
}

// NOLINTNEXTLINE
TEST_F(OptimizerContextTest, RecordOperatorNodeIntoGroupDuplicate) {
  auto context = OptimizerContext(nullptr);

  std::vector<std::unique_ptr<AbstractOptimizerNode>> c;
  auto tbl_free = std::make_unique<OperatorNode>(TableFreeScan::Make(), std::move(c))->Copy();

  GroupExpression *tbl_free_gexpr;
  EXPECT_TRUE(context.RecordOptimizerNodeIntoGroup(common::ManagedPointer(tbl_free), &tbl_free_gexpr));
  EXPECT_TRUE(tbl_free_gexpr != nullptr);

  // Duplicate should return false
  GroupExpression *dup_free_gexpr;
  EXPECT_TRUE(!context.RecordOptimizerNodeIntoGroup(common::ManagedPointer(tbl_free), &dup_free_gexpr));
  EXPECT_TRUE(dup_free_gexpr != nullptr);
  EXPECT_EQ(tbl_free_gexpr, dup_free_gexpr);
}

// NOLINTNEXTLINE
TEST_F(OptimizerContextTest, SimpleBindingTest) {
  auto context = OptimizerContext(nullptr);

  std::vector<std::unique_ptr<AbstractOptimizerNode>> c;
  auto left_get = std::make_unique<OperatorNode>(
      LogicalGet::Make(catalog::db_oid_t(1), catalog::namespace_oid_t(2), catalog::table_oid_t(3), {}, "tbl", false),
      std::move(c));
  auto right_get = std::unique_ptr<OperatorNode>(dynamic_cast<OperatorNode *>(left_get->Copy().release()));
  EXPECT_EQ(*left_get, *right_get);

  std::vector<std::unique_ptr<AbstractOptimizerNode>> jc;
  jc.emplace_back(std::move(left_get));
  jc.emplace_back(std::move(right_get));
  auto join = common::ManagedPointer<AbstractOptimizerNode>(new OperatorNode(LogicalInnerJoin::Make(), std::move(jc)));

  GroupExpression *gexpr = nullptr;
  EXPECT_TRUE(context.RecordOptimizerNodeIntoGroup(join, &gexpr));
  EXPECT_TRUE(gexpr != nullptr);

  auto *pattern = new Pattern(OpType::LOGICALINNERJOIN);
  pattern->AddChild(new Pattern(OpType::LOGICALGET));
  pattern->AddChild(new Pattern(OpType::LOGICALGET));

  auto *binding_iterator = new GroupExprBindingIterator(context.GetMemo(), gexpr, pattern);

  EXPECT_TRUE(binding_iterator->HasNext());

  auto binding = dynamic_cast<OperatorNode *>(binding_iterator->Next().release());

  auto &val1 = *binding;
  auto &val2 = *dynamic_cast<OperatorNode *>(join.Get());
  EXPECT_EQ(val1, val2);

  EXPECT_TRUE(!binding_iterator->HasNext());

  delete binding_iterator;
  delete pattern;
}

// NOLINTNEXTLINE
TEST_F(OptimizerContextTest, SingleWildcardTest) {
  auto context = OptimizerContext(nullptr);

  std::vector<std::unique_ptr<AbstractOptimizerNode>> c;
  auto left_get = std::make_unique<OperatorNode>(
      LogicalGet::Make(catalog::db_oid_t(1), catalog::namespace_oid_t(2), catalog::table_oid_t(3), {}, "tbl", false),
      std::move(c));
  auto right_get = left_get->Copy();
  EXPECT_EQ(*left_get, *reinterpret_cast<OperatorNode *>(right_get.get()));

  std::vector<std::unique_ptr<AbstractOptimizerNode>> jc;
  jc.emplace_back(std::move(left_get));
  jc.emplace_back(std::move(right_get));
  auto join = common::ManagedPointer<AbstractOptimizerNode>(new OperatorNode(LogicalInnerJoin::Make(), std::move(jc)));

  GroupExpression *gexpr = nullptr;
  EXPECT_TRUE(context.RecordOptimizerNodeIntoGroup(join, &gexpr));
  EXPECT_TRUE(gexpr != nullptr);

  auto *pattern = new Pattern(OpType::LOGICALINNERJOIN);
  pattern->AddChild(new Pattern(OpType::LEAF));
  pattern->AddChild(new Pattern(OpType::LEAF));

  auto *binding_iterator = new GroupExprBindingIterator(context.GetMemo(), gexpr, pattern);
  EXPECT_TRUE(binding_iterator->HasNext());

  auto binding = binding_iterator->Next();
  EXPECT_EQ(binding->Contents()->As<Operator>(), join->Contents()->As<Operator>());
  EXPECT_EQ(binding->GetChildren().size(), 2);

  auto left = binding->GetChildren()[0].CastManagedPointerTo<OperatorNode>();
  auto right = binding->GetChildren()[1].CastManagedPointerTo<OperatorNode>();
  EXPECT_TRUE(*left == *right);

  auto leaf = binding->GetChildren()[0]->Contents()->As<LeafOperator>();
  EXPECT_TRUE(leaf != nullptr);
  EXPECT_EQ(leaf->GetOriginGroup(), gexpr->GetChildGroupId(0));

  EXPECT_TRUE(!binding_iterator->HasNext());

  delete binding_iterator;
  delete pattern;
}

}  // namespace terrier::optimizer
