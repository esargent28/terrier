#pragma once

#include <memory>
#include <string>
#include <vector>
#include "common/hash_util.h"
#include "common/managed_pointer.h"
#include "optimizer/optimizer_defs.h"
#include "parser/expression_defs.h"

namespace terrier::optimizer {

class OperatorVisitor;

/**
 * Abstract class for the contents of expression-based and operator-based nodes
 * for the rewriter and optimizer, respectively.
 */
class AbstractOptimizerNodeContents {
 public:
  AbstractOptimizerNodeContents() = default;

  /**
   * Copy constructor.
   * @param contents The node contents to copy from.
   */
  explicit AbstractOptimizerNodeContents(common::ManagedPointer<AbstractOptimizerNodeContents> contents)
      : contents_(contents) {}

  ~AbstractOptimizerNodeContents() = default;

  /**
   * Accepts a visitor
   * @param v visitor
   */
  virtual void Accept(common::ManagedPointer<OperatorVisitor> v) const = 0;

  /**
   * @return Name of the node contents
   */
  virtual std::string GetName() const = 0;

  /**
   * @return OpType of the node contents
   */
  virtual OpType GetOpType() const = 0;

  /**
   * @return ExpressionType of the node contents
   */
  virtual parser::ExpressionType GetExpType() const = 0;

  /**
   * @return Whether node contents represent a physical operator / expression
   */
  virtual bool IsPhysical() const = 0;

  /**
   * @return Whether node represents a logical operator / expression
   */
  virtual bool IsLogical() const = 0;

  /**
   * Base definition of whether two AbstractOptimizerNodeContents objects are
   * equal -- simply checks whether OpType and ExpressionType match
   */
  virtual bool operator==(const AbstractOptimizerNodeContents &r) {
    return GetOpType() == r.GetOpType() && GetExpType() == r.GetExpType();
  }

  /**
   * @return True if the contained contents are non-null, false otherwise
   */
  virtual bool IsDefined() const { return contents_ != nullptr; }

  /**
   * Re-interpret the node contents's internal contents field
   * @tparam T the type of the node contents to be re-interpreted as
   * @return pointer to the re-interpreted node contents, nullptr if the types mismatch
   */
  template <typename T>
  common::ManagedPointer<T> As() const {
    if (contents_) {
      auto &n = *contents_;
      if (typeid(n) == typeid(T)) {
        return common::ManagedPointer<T>(dynamic_cast<T *>(contents_.Get()));
      }
    }
    return nullptr;
  }

  /**
   * Reinterpret the node contents itself (i.e. this object)
   * @tparam T The type of the object to be reinterpreted as
   * @return pointer to the reinterpreted object, nullptr if the types mismatch
   */
  template <typename T>
  common::ManagedPointer<const T> AsThis() const {
    auto &n = *this;
    if (typeid(n) == typeid(T)) {
      return common::ManagedPointer<const T>(dynamic_cast<const T *>(this));
    }
    return nullptr;
  }

 protected:
  common::ManagedPointer<AbstractOptimizerNodeContents> contents_;
};

}  // namespace terrier::optimizer