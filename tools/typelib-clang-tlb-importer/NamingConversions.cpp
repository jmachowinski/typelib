#include "NamingConversions.hpp"
#include "IgnoredOrRenamedTypes.hpp"

#include <iostream>

#include <clang/AST/DeclTemplate.h>
#include <clang/AST/PrettyPrinter.h>

// helperfunction: using the given string, looking up all instances of "from"
// and replacing them with "to" and returning the new string
std::string stringFromToReplace(const std::string &string,
                                const std::string &from,
                                const std::string &to) {

    std::string result(string);

    for (size_t start_pos = result.find(from); start_pos != std::string::npos;
         start_pos = result.find(from, start_pos + to.length())) {
        result.replace(start_pos, from.length(), to);
    }

    return result;
}

// the actual string-conversion function converting single cxx-names:
std::string cxxToTypelibName(const std::string& cxxName)
{
    std::string typelibName(cxxName);

    typelibName = stringFromToReplace(typelibName, "::", "/");
    // hmprf: this "hack" is still needed:
    typelibName = stringFromToReplace(typelibName, " [", "[");

    // reproducing the old gccxml behaviour. should not be needed as we do
    // everything correct already. remove this once everything settled enough.
    if (typelibName.at(0) != '/' && typelibName.length() > 1)
        typelibName.insert(0, 1, '/');

    return typelibName;
}

// helperfunction: convert a decl of a "template specialization" into a string in typelib-lingo
std::string
templateToTypelibName(const clang::ClassTemplateSpecializationDecl *tDecl) {

    if (!tDecl->getTemplateArgs().size())
        return "";

    std::string retval("<");

    const clang::TemplateArgumentList &tmpArgs(tDecl->getTemplateArgs());
    for (size_t idx = 0; idx < tmpArgs.size(); idx++) {

        std::string toBeAppended;
        switch (tmpArgs.get(idx).getKind()) {
        case clang::TemplateArgument::Declaration:
            /* std::cerr */
            /*     << " -- template arg declaration: " */
            /*     << tmpArgs.get(idx).getAsDecl()->getQualifiedNameAsString() << "\n"; */
            toBeAppended = cxxToTypelibName(
                tmpArgs.get(idx).getAsDecl()->getQualifiedNameAsString());
            break;
        case clang::TemplateArgument::Type: {
            std::string typelibNameOfTemplateArg = cxxToTypelibName(tmpArgs.get(idx).getAsType());
            /* std::cerr << " -- template arg type: " << typelibNameOfTemplateArg << "\n"; */
            toBeAppended = typelibNameOfTemplateArg;
        } break;
        case clang::TemplateArgument::Integral:
            /* std::cerr << " -- template arg integral: " */
            /*           << tmpArgs.get(idx).getAsIntegral().toString(10) << "\n"; */
            toBeAppended = tmpArgs.get(idx).getAsIntegral().toString(10);
            break;
        case clang::TemplateArgument::Template:
            std::cerr << " -- template arg template expansion... recursion? "
                         "not implemented yet... but should be easy\n";
            // toBeAppended = templateToTypelibName(tmpArgs.get(idx).getAsDecl());
            exit(EXIT_FAILURE);
            break;
        default:
            std::cerr << " -- template arg unhandled case "
                      << tmpArgs.get(idx).getKind() << "\n";
            exit(EXIT_FAILURE);
        }

        // having a "static vector<string>" somewhere containing
        // "to-be-skipped" template-arguments.
        if (IgnoredOrRenamedType::isTemplateArgumentIgnored(toBeAppended))
            continue;

        // we need commas as separator at certain places
        if (idx != 0)
            retval += "," + toBeAppended;
        else
            retval += toBeAppended;
    }

    // and finally close everything
    retval += ">";

    return retval;
}

std::string cxxToTypelibName(const clang::NamedDecl* decl)
{
    // just convert the name
    std::string typelibName(cxxToTypelibName(decl->getQualifiedNameAsString()));

    // note: template declarations as such and partial specializations are ignored here!
    if (const clang::ClassTemplateSpecializationDecl *tDecl =
            llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(decl)) {

        typelibName += templateToTypelibName(tDecl);
    }

    return typelibName;
}

// underlying types like "int" or "float[4]"
std::string cxxToTypelibName(const clang::QualType &type) {

    // this will give special handling for "Records" (e.g. classes, structs,
    // templates and so on)
    if (const clang::CXXRecordDecl *rDecl =
            type.getTypePtr()->getAsCXXRecordDecl()) {

        return cxxToTypelibName(rDecl);
    }

    // everything else is simply handled as a "Type" whose complete name is
    // printed into a string converted as-is
    clang::LangOptions o;
    clang::PrintingPolicy suppressTagKeyword(o);
    suppressTagKeyword.SuppressTagKeyword = true;

    return cxxToTypelibName(type.getAsString(suppressTagKeyword));
}

std::string typelibToCxxName(const std::string& typelibName)
{
    std::string cxxName = stringFromToReplace(typelibName, "/", "::");

    // this was done by the corresponding gccxml-ruby function. this will hide
    // if someone deliberately gave us classes or objects from the global
    // namespace... well, corner-cases... go along, nothing to see
    cxxName = stringFromToReplace(cxxName, "<::", "<");
    cxxName = stringFromToReplace(cxxName, ",::", ",");
    // note that one behaviour of the ruby-side was not ported -- adding a
    // space after a comma, like ',([^\s])' -> ', \1'

    // again, same behaviour which the gccxml-ruby code had.
    if ((cxxName.size() > 2) && (cxxName.substr(0,2) == std::string("::")))
        return cxxName.substr(2, std::string::npos);
    else
        return cxxName;
}

