// @(#)root/core/meta:$Id$
// Author: Paul Russo   30/07/2012

/*************************************************************************
 * Copyright (C) 1995-2000, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// TClingTypedefInfo                                                    //
//                                                                      //
// Emulation of the CINT TypedefInfo class.                             //
//                                                                      //
// The CINT C++ interpreter provides an interface to metadata about     //
// a typedef through the TypedefInfo class.  This class provides the    //
// same functionality, using an interface as close as possible to       //
// TypedefInfo but the typedef metadata comes from the Clang C++        //
// compiler, not CINT.                                                  //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "TClingTypedefInfo.h"

TClingTypedefInfo::~TClingTypedefInfo()
{
   delete fTypedefInfo;
   fTypedefInfo = 0;
   //fFirstTime = true;
   //fDescend = false;
   //fIter = clang::DeclContext::decl_iterator();
   fDecl = 0;
   //fIterStack.clear();
}

TClingTypedefInfo::TClingTypedefInfo(cling::Interpreter* interp)
   : fTypedefInfo(0), fInterp(interp), fFirstTime(true), fDescend(false),
     fDecl(0)
{
   fTypedefInfo = new G__TypedefInfo();
   if (gAllowClang) {
      const clang::TranslationUnitDecl* TU =
         fInterp->getCI()->getASTContext().getTranslationUnitDecl();
      const clang::DeclContext* DC = llvm::cast<clang::DeclContext>(TU);
      fIter = DC->decls_begin();
   }
}

TClingTypedefInfo::TClingTypedefInfo(cling::Interpreter* interp,
                                       const char* name)
   : fTypedefInfo(0), fInterp(interp), fFirstTime(true), fDescend(false),
     fDecl(0)
{
   if (!gAllowCint) {
      fTypedefInfo = new G__TypedefInfo();
   }
   else {
      fTypedefInfo = new G__TypedefInfo(name);
   }
   if (gAllowClang) {
      fDecl = const_cast<clang::Decl*>(fInterp->lookupScope(name));
      if (fDecl && !llvm::isa<clang::TypedefDecl>(fDecl)) {
         fDecl = 0;
      }
      if (fDecl) {
         AdvanceToDecl(fDecl);
      }
   }
}

TClingTypedefInfo::TClingTypedefInfo(const TClingTypedefInfo& rhs)
   : fTypedefInfo(0), fInterp(rhs.fInterp), fFirstTime(rhs.fFirstTime),
     fDescend(rhs.fDescend), fIter(rhs.fIter), fDecl(rhs.fDecl),
     fIterStack(rhs.fIterStack)
{
   fTypedefInfo = new G__TypedefInfo(*rhs.fTypedefInfo);
}

TClingTypedefInfo& TClingTypedefInfo::operator=(const TClingTypedefInfo& rhs)
{
   if (this != &rhs) {
      delete fTypedefInfo;
      fTypedefInfo = new G__TypedefInfo(*rhs.fTypedefInfo);
      fInterp = rhs.fInterp;
      fFirstTime = rhs.fFirstTime;
      fDescend = rhs.fDescend;
      fIter = rhs.fIter;
      fDecl = rhs.fDecl;
      fIterStack = rhs.fIterStack;
   }
   return *this;
}

G__TypedefInfo* TClingTypedefInfo::GetTypedefInfo() const
{
   return fTypedefInfo;
}

clang::Decl* TClingTypedefInfo::GetDecl() const
{
   return fDecl;
}

void TClingTypedefInfo::Init(const char* name)
{
   if (gDebug > 0) {
      fprintf(stderr,
              "TClingTypedefInfo::Init(name): looking up typedef: %s\n", name);
   }
   fFirstTime = true;
   fDescend = false;
   fIter = clang::DeclContext::decl_iterator();
   fDecl = 0;
   fIterStack.clear();
   if (gAllowCint) {
      fTypedefInfo->Init(name);
      if (gDebug > 0) {
         if (!fTypedefInfo->IsValid()) {
            fprintf(stderr,
                    "TClingTypedefInfo::Init(name): "
                    "could not find cint typedef for name: %s\n", name);
         }
         else {
            fprintf(stderr,
                    "TClingTypedefInfo::Init(name): "
                    "found cint typedef for name: %s  tagnum: %d\n",
                    name, fTypedefInfo->Tagnum());
         }
      }
   }
   if (gAllowClang) {
      const clang::Decl* decl = fInterp->lookupScope(name);
      if (!decl) {
         if (gDebug > 0) {
            fprintf(stderr,
                    "TClingTypedefInfo::Init(name): "
                    "cling typedef not found name: %s\n", name);
         }
         return;
      }
      fDecl = const_cast<clang::Decl*>(decl);
      if (gDebug > 0) {
         fprintf(stderr,
                 "TClingTypedefInfo::Init(name): "
                 "found cling typedef name: %s  decl: 0x%lx\n",
                 name, (long) fDecl);
      }
      AdvanceToDecl(fDecl);
   }
}

bool TClingTypedefInfo::IsValidCint() const
{
   if (gAllowCint) {
      return fTypedefInfo->IsValid();
   }
   return false;
}

bool TClingTypedefInfo::IsValidClang() const
{
   if (gAllowClang) {
      return fDecl;
   }
   return false;
}

bool TClingTypedefInfo::IsValid() const
{
   return IsValidCint() || IsValidClang();
}

int TClingTypedefInfo::AdvanceToDecl(const clang::Decl* target_decl)
{
   const clang::TranslationUnitDecl* TU = target_decl->getTranslationUnitDecl();
   const clang::DeclContext* DC = llvm::cast<clang::DeclContext>(TU);
   fFirstTime = true;
   fDescend = false;
   fIter = DC->decls_begin();
   fDecl = 0;
   fIterStack.clear();
   while (InternalNext()) {
      if (fDecl == target_decl) {
         return 1;
      }
   }
   return 0;
}

int TClingTypedefInfo::InternalNext()
{
   if (!*fIter) {
      // Iterator is already invalid.
      return 0;
   }
   while (true) {
      // Advance to next usable decl, or return if
      // there is no next usable decl.
      if (fFirstTime) {
         // The cint semantics are strange.
         fFirstTime = false;
      }
      else {
         // Advance the iterator one decl, descending into
         // the current decl context if necessary.
         if (!fDescend) {
            // Do not need to scan the decl context of the
            // current decl, move on to the next decl.
            ++fIter;
         }
         else {
            // Descend into the decl context of the current decl.
            fDescend = false;
            //fprintf(stderr,
            //   "TClingTypedefInfo::InternalNext: pushing ...\n");
            fIterStack.push_back(fIter);
            clang::DeclContext* DC = llvm::cast<clang::DeclContext>(*fIter);
            fIter = DC->decls_begin();
         }
         // Fix it if we went past the end.
         while (!*fIter && fIterStack.size()) {
            //fprintf(stderr,
            //   "TClingTypedefInfo::InternalNext: popping ...\n");
            fIter = fIterStack.back();
            fIterStack.pop_back();
            ++fIter;
         }
         // Check for final termination.
         if (!*fIter) {
            // We have reached the end of the translation unit, all done.
            fDecl = 0;
            return 0;
         }
      }
#if 0
      if (clang::NamedDecl* ND =
               llvm::dyn_cast<clang::NamedDecl>(*fIter)) {
         clang::ASTContext& Context = ND->getASTContext();
         clang::PrintingPolicy Policy(Context.getPrintingPolicy());
         std::string tmp;
         ND->getNameForDiagnostic(tmp, Policy, /*Qualified=*/true);
         fprintf(stderr,
                 "TClingTypedefInfo::InternalNext:  "
                 "0x%08lx %s  %s\n",
                 (long) *fIter, fIter->getDeclKindName(), tmp.c_str());
      }
#endif // 0
      // Return if this decl is a typedef.
      if (llvm::isa<clang::TypedefDecl>(*fIter)) {
         //if (const clang::TypedefDecl* TD =
         //      llvm::dyn_cast<clang::TypedefDecl>(*fIter)) {
         //clang::QualType QT = TD->getUnderlyingType();
         //if (const clang::RecordType* RT = QT->getAs<clang::RecordType>()) {
         //   if (!RT->getDecl()->getDefinition()) {
         //      // This is a typedef to a forward-declared type, skip it.
         //      continue;
         //   }
         //}
         // Iterator is now valid.
         fDecl = *fIter;
         return 1;
      }
      // Descend into namespaces and classes.
      clang::Decl::Kind DK = fIter->getKind();
      if ((DK == clang::Decl::Namespace) || (DK == clang::Decl::CXXRecord) ||
            (DK == clang::Decl::ClassTemplateSpecialization)) {
         fDescend = true;
      }
   }
}

int TClingTypedefInfo::Next()
{
   if (!gAllowClang) {
      return fTypedefInfo->Next();
   }
   return InternalNext();
}

long TClingTypedefInfo::Property() const
{
   if (!IsValid()) {
      return 0L;
   }
   if (!IsValidClang()) {
      if (gAllowCint) {
         return fTypedefInfo->Property();
      }
      return 0L;
   }
   if (!gAllowClang) {
      return 0L;
   }
   long property = 0L;
   property |= G__BIT_ISTYPEDEF;
   const clang::TypedefDecl* TD = llvm::dyn_cast<clang::TypedefDecl>(fDecl);
   clang::QualType QT = TD->getUnderlyingType().getCanonicalType();
   if (QT.isConstQualified()) {
      property |= G__BIT_ISCONSTANT;
   }
   while (1) {
      if (QT->isArrayType()) {
         QT = llvm::cast<clang::ArrayType>(QT)->getElementType();
         continue;
      }
      else if (QT->isReferenceType()) {
         property |= G__BIT_ISREFERENCE;
         QT = llvm::cast<clang::ReferenceType>(QT)->getPointeeType();
         continue;
      }
      else if (QT->isPointerType()) {
         property |= G__BIT_ISPOINTER;
         if (QT.isConstQualified()) {
            property |= G__BIT_ISPCONSTANT;
         }
         QT = llvm::cast<clang::PointerType>(QT)->getPointeeType();
         continue;
      }
      else if (QT->isMemberPointerType()) {
         QT = llvm::cast<clang::MemberPointerType>(QT)->getPointeeType();
         continue;
      }
      break;
   }
   if (QT->isBuiltinType()) {
      property |= G__BIT_ISFUNDAMENTAL;
   }
   if (QT.isConstQualified()) {
      property |= G__BIT_ISCONSTANT;
   }
   return property;
}

int TClingTypedefInfo::Size() const
{
   if (!IsValid()) {
      return 1;
   }
   if (!IsValidClang()) {
      if (gAllowCint) {
         return fTypedefInfo->Size();
      }
      return 1;
   }
   if (!gAllowClang) {
      return 1;
   }
   clang::ASTContext& Context = fDecl->getASTContext();
   const clang::TypedefDecl* TD = llvm::dyn_cast<clang::TypedefDecl>(fDecl);
   clang::QualType QT = TD->getUnderlyingType();
   if (QT->isDependentType()) {
      // The underlying type is dependent on a template parameter,
      // we have no idea what it is yet.
      return 0;
   }
   if (const clang::RecordType* RT = QT->getAs<clang::RecordType>()) {
      if (!RT->getDecl()->getDefinition()) {
         // This is a typedef to a forward-declared type.
         return 0;
      }
   }
   // Note: This is an int64_t.
   clang::CharUnits::QuantityType Quantity =
      Context.getTypeSizeInChars(QT).getQuantity();
   return static_cast<int>(Quantity);
}

const char* TClingTypedefInfo::TrueName() const
{
   if (!IsValid()) {
      return "(unknown)";
   }
   if (!IsValidClang()) {
      if (gAllowCint) {
         return fTypedefInfo->TrueName();
      }
      return "(unknown)";
   }
   if (!gAllowClang) {
      return "(unknown)";
   }
   // Note: This must be static because we return a pointer to the internals.
   static std::string truename;
   truename.clear();
   const clang::TypedefDecl* TD = llvm::dyn_cast<clang::TypedefDecl>(fDecl);
   truename = TD->getUnderlyingType().getAsString();
   return truename.c_str();
}

const char* TClingTypedefInfo::Name() const
{
   if (!IsValid()) {
      return "(unknown)";
   }
   if (!IsValidClang()) {
      if (gAllowCint) {
         return fTypedefInfo->Name();
      }
      return "(unknown)";
   }
   if (!gAllowClang) {
      return "(unknown)";
   }
   // Note: This must be static because we return a pointer to the internals.
   static std::string fullname;
   fullname.clear();
   clang::PrintingPolicy Policy(fDecl->getASTContext().getPrintingPolicy());
   llvm::dyn_cast<clang::NamedDecl>(fDecl)->
   getNameForDiagnostic(fullname, Policy, /*Qualified=*/true);
   return fullname.c_str();
}

const char* TClingTypedefInfo::Title() const
{
   if (!IsValid()) {
      return "";
   }
   if (!IsValidClang()) {
      if (gAllowCint) {
         return fTypedefInfo->Title();
      }
      return "";
   }
   if (!gAllowClang) {
      return "";
   }
   // FIXME: Implement when rootcling can provide it.
   return "";
}
