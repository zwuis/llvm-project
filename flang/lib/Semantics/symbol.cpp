//===-- lib/Semantics/symbol.cpp ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "flang/Semantics/symbol.h"
#include "flang/Common/idioms.h"
#include "flang/Evaluate/expression.h"
#include "flang/Semantics/scope.h"
#include "flang/Semantics/semantics.h"
#include "flang/Semantics/tools.h"
#include "llvm/Support/raw_ostream.h"
#include <cstring>
#include <string>
#include <type_traits>

namespace Fortran::semantics {

template <typename T>
static void DumpOptional(llvm::raw_ostream &os, const char *label, const T &x) {
  if (x) {
    os << ' ' << label << ':' << *x;
  }
}
template <typename T>
static void DumpExpr(llvm::raw_ostream &os, const char *label,
    const std::optional<evaluate::Expr<T>> &x) {
  if (x) {
    x->AsFortran(os << ' ' << label << ':');
  }
}

static void DumpBool(llvm::raw_ostream &os, const char *label, bool x) {
  if (x) {
    os << ' ' << label;
  }
}

static void DumpSymbolVector(llvm::raw_ostream &os, const SymbolVector &list) {
  char sep{' '};
  for (const Symbol &elem : list) {
    os << sep << elem.name();
    sep = ',';
  }
}

static void DumpType(llvm::raw_ostream &os, const Symbol &symbol) {
  if (const auto *type{symbol.GetType()}) {
    os << *type << ' ';
  }
}
static void DumpType(llvm::raw_ostream &os, const DeclTypeSpec *type) {
  if (type) {
    os << ' ' << *type;
  }
}

template <typename T>
static void DumpList(llvm::raw_ostream &os, const char *label, const T &list) {
  if (!list.empty()) {
    os << ' ' << label << ':';
    char sep{' '};
    for (const auto &elem : list) {
      os << sep << elem;
      sep = ',';
    }
  }
}

void SubprogramDetails::set_moduleInterface(Symbol &symbol) {
  CHECK(!moduleInterface_);
  moduleInterface_ = &symbol;
}

const Scope *ModuleDetails::parent() const {
  return isSubmodule_ && scope_ ? &scope_->parent() : nullptr;
}
const Scope *ModuleDetails::ancestor() const {
  return isSubmodule_ && scope_ ? FindModuleContaining(*scope_) : nullptr;
}
void ModuleDetails::set_scope(const Scope *scope) {
  CHECK(!scope_);
  bool scopeIsSubmodule{scope->parent().kind() == Scope::Kind::Module};
  CHECK(isSubmodule_ == scopeIsSubmodule);
  scope_ = scope;
}

llvm::raw_ostream &operator<<(
    llvm::raw_ostream &os, const SubprogramDetails &x) {
  DumpBool(os, "isInterface", x.isInterface_);
  DumpBool(os, "dummy", x.isDummy_);
  DumpOptional(os, "bindName", x.bindName());
  if (x.result_) {
    DumpType(os << " result:", x.result());
    os << x.result_->name();
    if (!x.result_->attrs().empty()) {
      os << ", " << x.result_->attrs();
    }
  }
  if (x.entryScope_) {
    os << " entry";
    if (x.entryScope_->symbol()) {
      os << " in " << x.entryScope_->symbol()->name();
    }
  }
  char sep{'('};
  os << ' ';
  for (const Symbol *arg : x.dummyArgs_) {
    os << sep;
    sep = ',';
    if (arg) {
      DumpType(os, *arg);
      os << arg->name();
    } else {
      os << '*';
    }
  }
  os << (sep == '(' ? "()" : ")");
  if (x.stmtFunction_) {
    os << " -> " << x.stmtFunction_->AsFortran();
  }
  if (x.moduleInterface_) {
    os << " moduleInterface: " << *x.moduleInterface_;
  }
  if (x.defaultIgnoreTKR_) {
    os << " defaultIgnoreTKR";
  }
  if (x.cudaSubprogramAttrs_) {
    os << " cudaSubprogramAttrs: "
       << common::EnumToString(*x.cudaSubprogramAttrs_);
  }
  if (!x.cudaLaunchBounds_.empty()) {
    os << " cudaLaunchBounds:";
    for (auto x : x.cudaLaunchBounds_) {
      os << ' ' << x;
    }
  }
  if (!x.cudaClusterDims_.empty()) {
    os << " cudaClusterDims:";
    for (auto x : x.cudaClusterDims_) {
      os << ' ' << x;
    }
  }
  if (!x.openACCRoutineInfos_.empty()) {
    os << " openACCRoutineInfos:";
    for (const auto &x : x.openACCRoutineInfos_) {
      os << x;
    }
  }
  return os;
}

llvm::raw_ostream &operator<<(
    llvm::raw_ostream &os, const OpenACCRoutineDeviceTypeInfo &x) {
  if (x.dType() != common::OpenACCDeviceType::None) {
    os << " deviceType(" << common::EnumToString(x.dType()) << ')';
  }
  if (x.isSeq()) {
    os << " seq";
  }
  if (x.isVector()) {
    os << " vector";
  }
  if (x.isWorker()) {
    os << " worker";
  }
  if (x.isGang()) {
    os << " gang(" << x.gangDim() << ')';
  }
  if (const auto *bindName{x.bindName()}) {
    if (const auto &symbol{std::get_if<std::string>(bindName)}) {
      os << " bindName(\"" << *symbol << "\")";
    } else {
      const SymbolRef s{std::get<SymbolRef>(*bindName)};
      os << " bindName(" << s->name() << ")";
    }
  }
  return os;
}

llvm::raw_ostream &operator<<(
    llvm::raw_ostream &os, const OpenACCRoutineInfo &x) {
  if (x.isNohost()) {
    os << " nohost";
  }
  os << static_cast<const OpenACCRoutineDeviceTypeInfo &>(x);
  for (const auto &d : x.deviceTypeInfos_) {
    os << d;
  }
  return os;
}

void EntityDetails::set_type(const DeclTypeSpec &type) {
  CHECK(!type_);
  type_ = &type;
}

void AssocEntityDetails::set_rank(int rank) { rank_ = rank; }
void AssocEntityDetails::set_IsAssumedSize() { rank_ = isAssumedSize; }
void AssocEntityDetails::set_IsAssumedRank() { rank_ = isAssumedRank; }
void AssocEntityDetails::set_isTypeGuard(bool yes) { isTypeGuard_ = yes; }
void EntityDetails::ReplaceType(const DeclTypeSpec &type) { type_ = &type; }

ObjectEntityDetails::ObjectEntityDetails(EntityDetails &&d)
    : EntityDetails(std::move(d)) {}

void ObjectEntityDetails::set_shape(const ArraySpec &shape) {
  CHECK(shape_.empty());
  for (const auto &shapeSpec : shape) {
    shape_.push_back(shapeSpec);
  }
}
void ObjectEntityDetails::set_coshape(const ArraySpec &coshape) {
  CHECK(coshape_.empty());
  for (const auto &shapeSpec : coshape) {
    coshape_.push_back(shapeSpec);
  }
}

ProcEntityDetails::ProcEntityDetails(EntityDetails &&d)
    : EntityDetails(std::move(d)) {}

UseErrorDetails::UseErrorDetails(const UseDetails &useDetails) {
  add_occurrence(useDetails.location(), useDetails.symbol());
}
UseErrorDetails &UseErrorDetails::add_occurrence(
    const SourceName &location, const Symbol &used) {
  occurrences_.push_back(std::make_pair(location, &used));
  return *this;
}

void GenericDetails::AddSpecificProc(
    const Symbol &proc, SourceName bindingName) {
  specificProcs_.push_back(proc);
  bindingNames_.push_back(bindingName);
}
void GenericDetails::set_specific(Symbol &specific) {
  CHECK(!specific_);
  specific_ = &specific;
}
void GenericDetails::clear_specific() { specific_ = nullptr; }
void GenericDetails::set_derivedType(Symbol &derivedType) {
  CHECK(!derivedType_);
  derivedType_ = &derivedType;
}
void GenericDetails::clear_derivedType() { derivedType_ = nullptr; }
void GenericDetails::AddUse(const Symbol &use) {
  CHECK(use.has<UseDetails>());
  uses_.push_back(use);
}

const Symbol *GenericDetails::CheckSpecific() const {
  return const_cast<GenericDetails *>(this)->CheckSpecific();
}
Symbol *GenericDetails::CheckSpecific() {
  if (specific_ && !specific_->has<UseErrorDetails>()) {
    const Symbol &ultimate{specific_->GetUltimate()};
    for (const Symbol &proc : specificProcs_) {
      if (&proc.GetUltimate() == &ultimate) {
        return nullptr;
      }
    }
    return specific_;
  } else {
    return nullptr;
  }
}

void GenericDetails::CopyFrom(const GenericDetails &from) {
  CHECK(specificProcs_.size() == bindingNames_.size());
  CHECK(from.specificProcs_.size() == from.bindingNames_.size());
  kind_ = from.kind_;
  if (from.derivedType_) {
    CHECK(!derivedType_ || derivedType_ == from.derivedType_);
    derivedType_ = from.derivedType_;
  }
  for (std::size_t i{0}; i < from.specificProcs_.size(); ++i) {
    if (llvm::none_of(specificProcs_, [&](const Symbol &mySymbol) {
          return &mySymbol.GetUltimate() ==
              &from.specificProcs_[i]->GetUltimate();
        })) {
      specificProcs_.push_back(from.specificProcs_[i]);
      bindingNames_.push_back(from.bindingNames_[i]);
    }
  }
}

// The name of the kind of details for this symbol.
// This is primarily for debugging.
std::string DetailsToString(const Details &details) {
  return common::visit(
      common::visitors{[](const UnknownDetails &) { return "Unknown"; },
          [](const MainProgramDetails &) { return "MainProgram"; },
          [](const ModuleDetails &) { return "Module"; },
          [](const SubprogramDetails &) { return "Subprogram"; },
          [](const SubprogramNameDetails &) { return "SubprogramName"; },
          [](const EntityDetails &) { return "Entity"; },
          [](const ObjectEntityDetails &) { return "ObjectEntity"; },
          [](const ProcEntityDetails &) { return "ProcEntity"; },
          [](const DerivedTypeDetails &) { return "DerivedType"; },
          [](const UseDetails &) { return "Use"; },
          [](const UseErrorDetails &) { return "UseError"; },
          [](const HostAssocDetails &) { return "HostAssoc"; },
          [](const GenericDetails &) { return "Generic"; },
          [](const ProcBindingDetails &) { return "ProcBinding"; },
          [](const NamelistDetails &) { return "Namelist"; },
          [](const CommonBlockDetails &) { return "CommonBlockDetails"; },
          [](const TypeParamDetails &) { return "TypeParam"; },
          [](const MiscDetails &) { return "Misc"; },
          [](const AssocEntityDetails &) { return "AssocEntity"; },
          [](const UserReductionDetails &) { return "UserReductionDetails"; }},
      details);
}

std::string Symbol::GetDetailsName() const { return DetailsToString(details_); }

void Symbol::set_details(Details &&details) {
  CHECK(CanReplaceDetails(details));
  details_ = std::move(details);
}

bool Symbol::CanReplaceDetails(const Details &details) const {
  if (has<UnknownDetails>()) {
    return true; // can always replace UnknownDetails
  } else {
    return common::visit(
        common::visitors{
            [](const UseErrorDetails &) { return true; },
            [&](const ObjectEntityDetails &) { return has<EntityDetails>(); },
            [&](const ProcEntityDetails &) { return has<EntityDetails>(); },
            [&](const SubprogramDetails &) {
              return has<SubprogramNameDetails>() || has<EntityDetails>();
            },
            [&](const DerivedTypeDetails &) {
              const auto *derived{this->detailsIf<DerivedTypeDetails>()};
              return derived && derived->isForwardReferenced();
            },
            [&](const UseDetails &x) {
              const auto *use{this->detailsIf<UseDetails>()};
              return use && use->symbol() == x.symbol();
            },
            [&](const HostAssocDetails &) {
              return this->has<HostAssocDetails>();
            },
            [&](const UserReductionDetails &) {
              return this->has<UserReductionDetails>();
            },
            [](const auto &) { return false; },
        },
        details);
  }
}

// Usually a symbol's name is the first occurrence in the source, but sometimes
// we want to replace it with one at a different location (but same characters).
void Symbol::ReplaceName(const SourceName &name) {
  CHECK(name == name_);
  name_ = name;
}

void Symbol::SetType(const DeclTypeSpec &type) {
  common::visit(common::visitors{
                    [&](EntityDetails &x) { x.set_type(type); },
                    [&](ObjectEntityDetails &x) { x.set_type(type); },
                    [&](AssocEntityDetails &x) { x.set_type(type); },
                    [&](ProcEntityDetails &x) { x.set_type(type); },
                    [&](TypeParamDetails &x) { x.set_type(type); },
                    [](auto &) {},
                },
      details_);
}

template <typename T>
constexpr bool HasBindName{std::is_convertible_v<T, const WithBindName *>};

const std::string *Symbol::GetBindName() const {
  return common::visit(
      [&](auto &x) -> const std::string * {
        if constexpr (HasBindName<decltype(&x)>) {
          return x.bindName();
        } else {
          return nullptr;
        }
      },
      details_);
}

void Symbol::SetBindName(std::string &&name) {
  common::visit(
      [&](auto &x) {
        if constexpr (HasBindName<decltype(&x)>) {
          x.set_bindName(std::move(name));
        } else {
          DIE("bind name not allowed on this kind of symbol");
        }
      },
      details_);
}

bool Symbol::GetIsExplicitBindName() const {
  return common::visit(
      [&](auto &x) -> bool {
        if constexpr (HasBindName<decltype(&x)>) {
          return x.isExplicitBindName();
        } else {
          return false;
        }
      },
      details_);
}

void Symbol::SetIsExplicitBindName(bool yes) {
  common::visit(
      [&](auto &x) {
        if constexpr (HasBindName<decltype(&x)>) {
          x.set_isExplicitBindName(yes);
        } else {
          DIE("bind name not allowed on this kind of symbol");
        }
      },
      details_);
}

void Symbol::SetIsCDefined(bool yes) {
  common::visit(
      [&](auto &x) {
        if constexpr (HasBindName<decltype(&x)>) {
          x.set_isCDefined(yes);
        } else {
          DIE("CDEFINED not allowed on this kind of symbol");
        }
      },
      details_);
}

bool Symbol::IsFuncResult() const {
  return common::visit(
      common::visitors{[](const EntityDetails &x) { return x.isFuncResult(); },
          [](const ObjectEntityDetails &x) { return x.isFuncResult(); },
          [](const ProcEntityDetails &x) { return x.isFuncResult(); },
          [](const HostAssocDetails &x) { return x.symbol().IsFuncResult(); },
          [](const auto &) { return false; }},
      details_);
}

const ArraySpec *Symbol::GetShape() const {
  if (const auto *details{std::get_if<ObjectEntityDetails>(&details_)}) {
    return &details->shape();
  } else {
    return nullptr;
  }
}

bool Symbol::IsObjectArray() const {
  const ArraySpec *shape{GetShape()};
  return shape && !shape->empty();
}

bool Symbol::IsSubprogram() const {
  return common::visit(
      common::visitors{
          [](const SubprogramDetails &) { return true; },
          [](const SubprogramNameDetails &) { return true; },
          [](const GenericDetails &) { return true; },
          [](const UseDetails &x) { return x.symbol().IsSubprogram(); },
          [](const auto &) { return false; },
      },
      details_);
}

bool Symbol::IsFromModFile() const {
  return test(Flag::ModFile) ||
      (!owner_->IsTopLevel() && owner_->symbol()->IsFromModFile());
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const EntityDetails &x) {
  DumpBool(os, "dummy", x.isDummy());
  DumpBool(os, "funcResult", x.isFuncResult());
  if (x.type()) {
    os << " type: " << *x.type();
  }
  DumpOptional(os, "bindName", x.bindName());
  DumpBool(os, "CDEFINED", x.isCDefined());
  return os;
}

llvm::raw_ostream &operator<<(
    llvm::raw_ostream &os, const ObjectEntityDetails &x) {
  os << *static_cast<const EntityDetails *>(&x);
  DumpList(os, "shape", x.shape());
  DumpList(os, "coshape", x.coshape());
  DumpExpr(os, "init", x.init_);
  if (x.unanalyzedPDTComponentInit()) {
    os << " (has unanalyzedPDTComponentInit)";
  }
  if (!x.ignoreTKR_.empty()) {
    x.ignoreTKR_.Dump(os << ' ', common::EnumToString);
  }
  if (x.cudaDataAttr()) {
    os << " cudaDataAttr: " << common::EnumToString(*x.cudaDataAttr());
  }
  return os;
}

llvm::raw_ostream &operator<<(
    llvm::raw_ostream &os, const AssocEntityDetails &x) {
  os << *static_cast<const EntityDetails *>(&x);
  if (x.IsAssumedSize()) {
    os << " RANK(*)";
  } else if (x.IsAssumedRank()) {
    os << " RANK DEFAULT";
  } else if (auto assocRank{x.rank()}) {
    os << " RANK(" << *assocRank << ')';
  }
  DumpExpr(os, "expr", x.expr());
  return os;
}

llvm::raw_ostream &operator<<(
    llvm::raw_ostream &os, const ProcEntityDetails &x) {
  if (x.procInterface_) {
    if (x.rawProcInterface_ != x.procInterface_) {
      os << ' ' << x.rawProcInterface_->name() << " ->";
    }
    os << ' ' << x.procInterface_->name();
  } else {
    DumpType(os, x.type());
  }
  DumpOptional(os, "bindName", x.bindName());
  DumpOptional(os, "passName", x.passName());
  if (x.init()) {
    if (const Symbol * target{*x.init()}) {
      os << " => " << target->name();
    } else {
      os << " => NULL()";
    }
  }
  if (x.isCUDAKernel()) {
    os << " isCUDAKernel";
  }
  return os;
}

llvm::raw_ostream &operator<<(
    llvm::raw_ostream &os, const DerivedTypeDetails &x) {
  DumpBool(os, "sequence", x.sequence_);
  DumpList(os, "components", x.componentNames_);
  return os;
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const GenericDetails &x) {
  os << ' ' << x.kind().ToString();
  DumpBool(os, "(specific)", x.specific() != nullptr);
  DumpBool(os, "(derivedType)", x.derivedType() != nullptr);
  if (const auto &uses{x.uses()}; !uses.empty()) {
    os << " (uses:";
    char sep{' '};
    for (const Symbol &use : uses) {
      const Symbol &ultimate{use.GetUltimate()};
      os << sep << ultimate.name() << "->"
         << ultimate.owner().GetName().value();
      sep = ',';
    }
    os << ')';
  }
  os << " procs:";
  DumpSymbolVector(os, x.specificProcs());
  return os;
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const Details &details) {
  os << DetailsToString(details);
  common::visit( //
      common::visitors{
          [&](const UnknownDetails &) {},
          [&](const MainProgramDetails &) {},
          [&](const ModuleDetails &x) {
            if (x.isSubmodule()) {
              os << " (";
              if (x.ancestor()) {
                auto ancestor{x.ancestor()->GetName().value()};
                os << ancestor;
                if (x.parent()) {
                  auto parent{x.parent()->GetName().value()};
                  if (ancestor != parent) {
                    os << ':' << parent;
                  }
                }
              }
              os << ")";
            }
            if (x.isDefaultPrivate()) {
              os << " isDefaultPrivate";
            }
          },
          [&](const SubprogramNameDetails &x) {
            os << ' ' << EnumToString(x.kind());
          },
          [&](const UseDetails &x) {
            os << " from " << x.symbol().name() << " in "
               << GetUsedModule(x).name();
          },
          [&](const UseErrorDetails &x) {
            os << " uses:";
            char sep{':'};
            for (const auto &[location, sym] : x.occurrences()) {
              os << sep << " from " << sym->name() << " at " << location;
              sep = ',';
            }
          },
          [](const HostAssocDetails &) {},
          [&](const ProcBindingDetails &x) {
            os << " => " << x.symbol().name();
            DumpOptional(os, "passName", x.passName());
            if (x.numPrivatesNotOverridden() > 0) {
              os << " numPrivatesNotOverridden: "
                 << x.numPrivatesNotOverridden();
            }
          },
          [&](const NamelistDetails &x) {
            os << ':';
            DumpSymbolVector(os, x.objects());
          },
          [&](const CommonBlockDetails &x) {
            DumpOptional(os, "bindName", x.bindName());
            if (x.alignment()) {
              os << " alignment=" << x.alignment();
            }
            os << ':';
            for (const auto &object : x.objects()) {
              os << ' ' << object->name();
            }
          },
          [&](const TypeParamDetails &x) {
            DumpOptional(os, "type", x.type());
            if (auto attr{x.attr()}) {
              os << ' ' << common::EnumToString(*attr);
            } else {
              os << " (no attr)";
            }
            DumpExpr(os, "init", x.init());
          },
          [&](const MiscDetails &x) {
            os << ' ' << MiscDetails::EnumToString(x.kind());
          },
          [&](const UserReductionDetails &x) {
            for (auto &type : x.GetTypeList()) {
              DumpType(os, type);
            }
          },
          [&](const auto &x) { os << x; },
      },
      details);
  return os;
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &o, Symbol::Flag flag) {
  return o << Symbol::EnumToString(flag);
}

llvm::raw_ostream &operator<<(
    llvm::raw_ostream &o, const Symbol::Flags &flags) {
  std::size_t n{flags.count()};
  std::size_t seen{0};
  for (std::size_t j{0}; seen < n; ++j) {
    Symbol::Flag flag{static_cast<Symbol::Flag>(j)};
    if (flags.test(flag)) {
      if (seen++ > 0) {
        o << ", ";
      }
      o << flag;
    }
  }
  return o;
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const Symbol &symbol) {
  os << symbol.name();
  if (!symbol.attrs().empty()) {
    os << ", " << symbol.attrs();
  }
  if (!symbol.flags().empty()) {
    os << " (" << symbol.flags() << ')';
  }
  if (symbol.size_) {
    os << " size=" << symbol.size_ << " offset=" << symbol.offset_;
  }
  os << ": " << symbol.details_;
  return os;
}

#if !defined(NDEBUG) || defined(LLVM_ENABLE_DUMP)
void Symbol::dump() const { llvm::errs() << *this << '\n'; }
#endif

// Output a unique name for a scope by qualifying it with the names of
// parent scopes. For scopes without corresponding symbols, use the kind
// with an index (e.g. Block1, Block2, etc.).
static void DumpUniqueName(llvm::raw_ostream &os, const Scope &scope) {
  if (!scope.IsTopLevel()) {
    DumpUniqueName(os, scope.parent());
    os << '/';
    if (auto *scopeSymbol{scope.symbol()};
        scopeSymbol && !scopeSymbol->name().empty()) {
      os << scopeSymbol->name();
    } else {
      int index{1};
      for (auto &child : scope.parent().children()) {
        if (child == scope) {
          break;
        }
        if (child.kind() == scope.kind()) {
          ++index;
        }
      }
      os << Scope::EnumToString(scope.kind()) << index;
    }
  }
}

// Dump a symbol for UnparseWithSymbols. This will be used for tests so the
// format should be reasonably stable.
llvm::raw_ostream &DumpForUnparse(
    llvm::raw_ostream &os, const Symbol &symbol, bool isDef) {
  DumpUniqueName(os, symbol.owner());
  os << '/' << symbol.name();
  if (isDef) {
    if (!symbol.attrs().empty()) {
      os << ' ' << symbol.attrs();
    }
    if (!symbol.flags().empty()) {
      os << " (" << symbol.flags() << ')';
    }
    os << ' ' << symbol.GetDetailsName();
    DumpType(os, symbol.GetType());
  }
  return os;
}

const DerivedTypeSpec *Symbol::GetParentTypeSpec(const Scope *scope) const {
  if (const Symbol * parentComponent{GetParentComponent(scope)}) {
    const auto &object{parentComponent->get<ObjectEntityDetails>()};
    return &object.type()->derivedTypeSpec();
  } else {
    return nullptr;
  }
}

const Symbol *Symbol::GetParentComponent(const Scope *scope) const {
  if (const auto *dtDetails{detailsIf<DerivedTypeDetails>()}) {
    if (const Scope * localScope{scope ? scope : scope_}) {
      return dtDetails->GetParentComponent(DEREF(localScope));
    }
  }
  return nullptr;
}

void DerivedTypeDetails::add_component(const Symbol &symbol) {
  if (symbol.test(Symbol::Flag::ParentComp)) {
    CHECK(componentNames_.empty());
  }
  componentNames_.push_back(symbol.name());
}

const Symbol *DerivedTypeDetails::GetParentComponent(const Scope &scope) const {
  if (auto extends{GetParentComponentName()}) {
    if (auto iter{scope.find(*extends)}; iter != scope.cend()) {
      if (const Symbol & symbol{*iter->second};
          symbol.test(Symbol::Flag::ParentComp)) {
        return &symbol;
      }
    }
  }
  return nullptr;
}

const Symbol *DerivedTypeDetails::GetFinalForRank(int rank) const {
  for (const auto &pair : finals_) {
    const Symbol &symbol{*pair.second};
    if (const auto *details{symbol.detailsIf<SubprogramDetails>()}) {
      if (details->dummyArgs().size() == 1) {
        if (const Symbol * arg{details->dummyArgs().at(0)}) {
          if (const auto *object{arg->detailsIf<ObjectEntityDetails>()}) {
            if (rank == object->shape().Rank() || object->IsAssumedRank() ||
                IsElementalProcedure(symbol)) {
              return &symbol;
            }
          }
        }
      }
    }
  }
  return nullptr;
}

TypeParamDetails &TypeParamDetails::set_attr(common::TypeParamAttr attr) {
  CHECK(!attr_);
  attr_ = attr;
  return *this;
}

TypeParamDetails &TypeParamDetails::set_type(const DeclTypeSpec &type) {
  CHECK(!type_);
  type_ = &type;
  return *this;
}

bool GenericKind::IsIntrinsicOperator() const {
  return Is(OtherKind::Concat) || Has<common::LogicalOperator>() ||
      Has<common::NumericOperator>() || Has<common::RelationalOperator>();
}

bool GenericKind::IsOperator() const {
  return IsDefinedOperator() || IsIntrinsicOperator();
}

std::string GenericKind::ToString() const {
  return common::visit(
      common::visitors{
          [](const OtherKind &x) { return std::string{EnumToString(x)}; },
          [](const common::DefinedIo &x) { return AsFortran(x).ToString(); },
          [](const auto &x) { return std::string{common::EnumToString(x)}; },
      },
      u);
}

SourceName GenericKind::AsFortran(common::DefinedIo x) {
  const char *name{common::AsFortran(x)};
  return {name, std::strlen(name)};
}

bool GenericKind::Is(GenericKind::OtherKind x) const {
  const OtherKind *y{std::get_if<OtherKind>(&u)};
  return y && *y == x;
}

std::string Symbol::OmpFlagToClauseName(Symbol::Flag ompFlag) {
  std::string clauseName;
  switch (ompFlag) {
  case Symbol::Flag::OmpShared:
    clauseName = "SHARED";
    break;
  case Symbol::Flag::OmpPrivate:
    clauseName = "PRIVATE";
    break;
  case Symbol::Flag::OmpLinear:
    clauseName = "LINEAR";
    break;
  case Symbol::Flag::OmpUniform:
    clauseName = "UNIFORM";
    break;
  case Symbol::Flag::OmpFirstPrivate:
    clauseName = "FIRSTPRIVATE";
    break;
  case Symbol::Flag::OmpLastPrivate:
    clauseName = "LASTPRIVATE";
    break;
  case Symbol::Flag::OmpMapTo:
  case Symbol::Flag::OmpMapFrom:
  case Symbol::Flag::OmpMapToFrom:
  case Symbol::Flag::OmpMapStorage:
  case Symbol::Flag::OmpMapDelete:
    clauseName = "MAP";
    break;
  case Symbol::Flag::OmpUseDevicePtr:
    clauseName = "USE_DEVICE_PTR";
    break;
  case Symbol::Flag::OmpUseDeviceAddr:
    clauseName = "USE_DEVICE_ADDR";
    break;
  case Symbol::Flag::OmpCopyIn:
    clauseName = "COPYIN";
    break;
  case Symbol::Flag::OmpCopyPrivate:
    clauseName = "COPYPRIVATE";
    break;
  case Symbol::Flag::OmpIsDevicePtr:
    clauseName = "IS_DEVICE_PTR";
    break;
  case Symbol::Flag::OmpHasDeviceAddr:
    clauseName = "HAS_DEVICE_ADDR";
    break;
  default:
    clauseName = "";
    break;
  }
  return clauseName;
}

bool SymbolOffsetCompare::operator()(
    const SymbolRef &x, const SymbolRef &y) const {
  const Symbol *xCommon{FindCommonBlockContaining(*x)};
  const Symbol *yCommon{FindCommonBlockContaining(*y)};
  if (xCommon) {
    if (yCommon) {
      const SymbolSourcePositionCompare sourceCmp;
      if (sourceCmp(*xCommon, *yCommon)) {
        return true;
      } else if (sourceCmp(*yCommon, *xCommon)) {
        return false;
      } else if (x->offset() == y->offset()) {
        return x->size() > y->size();
      } else {
        return x->offset() < y->offset();
      }
    } else {
      return false;
    }
  } else if (yCommon) {
    return true;
  } else if (x->offset() == y->offset()) {
    return x->size() > y->size();
  } else {
    return x->offset() < y->offset();
  }
  return x->GetSemanticsContext().allCookedSources().Precedes(
      x->name(), y->name());
}

bool SymbolOffsetCompare::operator()(
    const MutableSymbolRef &x, const MutableSymbolRef &y) const {
  return (*this)(SymbolRef{*x}, SymbolRef{*y});
}

} // namespace Fortran::semantics
