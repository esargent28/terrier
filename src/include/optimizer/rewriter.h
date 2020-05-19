#pragma once

#include <memory>

#include "optimizer/abstract_optimizer_node.h"
#include "optimizer/optimizer_context.h"
#include "optimizer/optimizer_task_pool.h"
#include "parser/expression/abstract_expression.h"
#include "transaction/transaction_context.h"

namespace terrier::optimizer {

/**
 * The syntax-based query rewriter. Takes an abstract expression and rewrites
 *   it based on the rules in its rule set.
 */
class Rewriter {
 public:
  /**
   * Constructor.
   * @param txn the transaction context being used for memory management
   */
  explicit Rewriter(transaction::TransactionContext *txn);

  /**
   * Destructor.
   */
  ~Rewriter();

  /**
   * Resets the internal state of the rewriter
   * @param txn the transaction context being used for memory management
   */
  void Reset(transaction::TransactionContext *txn);

  /**
   * Shouldn't be able to copy/move the rewriter
   */
  DISALLOW_COPY_AND_MOVE(Rewriter);

  /**
   * Sets the optimizer context's transaction context to the one provided.
   * @param txn the transaction context to use
   */
  void SetTxn(transaction::TransactionContext *txn) { context_->SetTxn(txn); }

  /**
   * Gets the OptimizerMetadata used by the rewriter
   * @returns internal OptimizerMetadata
   */
  OptimizerContext *GetContext() { return context_; }

  /**
   * Rewrites an expression by applying applicable rules
   * @param expr AbstractExpression to rewrite
   * @returns rewritten AbstractExpression
   */
  common::ManagedPointer<parser::AbstractExpression> RewriteExpression(
      common::ManagedPointer<parser::AbstractExpression> expr);

 private:
  /**
   * Creates an AbstractExpression from the Memo used internally
   * @param root_group GroupID of the root group to begin building from
   * @returns AbstractExpression from the stored groups
   */
  common::ManagedPointer<parser::AbstractExpression> RebuildExpression(group_id_t root_group);

  /**
   * Performs a single rewrite pass on the expression
   * @param root_group_id GroupID of the group to start rewriting from
   */
  void RewriteLoop(group_id_t root_group_id);

  /**
   * Converts AbstractExpression into internal rewriter representation
   * @param expr expression to convert
   * @returns shared pointer to rewriter internal representation
   */
  std::unique_ptr<AbstractOptimizerNode> ConvertToOptimizerNode(
      common::ManagedPointer<parser::AbstractExpression> expr);

  /**
   * Records the original groups (subtrees) of the AbstractExpression.
   * From the recorded information, it is possible to rebuild the expression.
   * @param expr expression whose groups to record
   * @returns GroupExpression representing the root of the expression
   */
  common::ManagedPointer<GroupExpression> RecordTreeGroups(common::ManagedPointer<parser::AbstractExpression> expr);

  /**
   * OptimizerContext that we leverage
   */
  OptimizerContext *context_;
};

}  // namespace terrier::optimizer