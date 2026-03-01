/*
 *  File Name: preprocessor.cpp
 *  Description: Renames variables in the AST so each declared variable has a unique name.
 *  Author: Papa Yaw Owusu Nti
 */

#include "preprocessor.h"

#include <cstdlib>
#include <cstring>

#include <string>
#include <unordered_map>
#include <vector>

/* Scope stack: each scope maps original name -> unique name. */
static std::vector<std::unordered_map<std::string, std::string>> scope_stack;

/* Unique suffix counter for generated names. */
static unsigned long long unique_id = 0;

/* Allocate a new C-string copy from a std::string (caller frees with free). */
static char* dupCString(const std::string& s) {
    char* out = (char*)malloc(s.size() + 1);
    if (!out) return nullptr;
    std::strcpy(out, s.c_str());
    return out;
}

/* Replace an AST name field with a newly allocated C-string. */
static void setAstName(char* &field, const std::string& newName) {
    if (field) {
        free(field);
        field = nullptr;
    }
    field = dupCString(newName);
}

/* Push a new empty scope onto the scope stack. */
static void enterScope() {
    scope_stack.push_back(std::unordered_map<std::string, std::string>());
}

/* Pop the current scope from the scope stack. */
static void exitScope() {
    if (!scope_stack.empty()) {
        scope_stack.pop_back();
    }
}

/* Build a unique name using the original name plus a numeric suffix. */
static std::string makeUniqueName(const char* original) {
    std::string base = (original == nullptr) ? "" : std::string(original);
    return base + "$" + std::to_string(unique_id++);
}

/* Declare a variable in the current scope and return its unique name. */
static std::string declareUnique(const char* original) {
    if (scope_stack.empty()) {
        enterScope();
    }

    std::string key = (original == nullptr) ? "" : std::string(original);
    std::string unique = makeUniqueName(original);

    scope_stack.back()[key] = unique;
    return unique;
}

/* Resolve a variable use to its unique name */
static std::string resolveUnique(const char* name) {
    std::string key = (name == nullptr) ? "" : std::string(name);

    for (int i = (int)scope_stack.size() - 1; i >= 0; --i) {
        auto it = scope_stack[i].find(key);
        if (it != scope_stack[i].end()) {
            return it->second;
        }
    }

    // If semantic analysis passed, this should not happen
    return "";
}

/* Rename variables throughout an AST subtree. */
static void renameNode(astNode* node);

/* Rename all statements inside a block statement node. */
static void renameStmtList(astNode* stmtNode) {
    if (!stmtNode) return;

    // If this is a block statement, rename each statement in its list
    if (stmtNode->type == ast_stmt && stmtNode->stmt.type == ast_block) {
        std::vector<astNode*>* list = stmtNode->stmt.block.stmt_list;
        if (!list) return;

        for (size_t i = 0; i < list->size(); ++i) {
            renameNode((*list)[i]);
        }
        return;
    }

    // Otherwise, just rename this single node
    renameNode(stmtNode);
}

/* Rename variables inside a statement node. */
static void renameStatement(astNode* node) {
    if (!node || node->type != ast_stmt) return;

    // Rename based on the statement kind
    switch (node->stmt.type) {
        case ast_decl: {
            // Declaration: create and store a unique name
            std::string unique = declareUnique(node->stmt.decl.name);
            setAstName(node->stmt.decl.name, unique);
            break;
        }

        case ast_asgn:
            // Assignment: rename LHS variable and rename RHS expression
            renameNode(node->stmt.asgn.lhs);
            renameNode(node->stmt.asgn.rhs);
            break;

        case ast_call:
            // Call statement: rename the parameter expression (print has a parameter)
            if (node->stmt.call.param) {
                renameNode(node->stmt.call.param);
            }
            break;

        case ast_ret:
            // Return: rename return expression
            renameNode(node->stmt.ret.expr);
            break;

        case ast_if:
            // If / if-else: rename condition and rename both bodies
            renameNode(node->stmt.ifn.cond);
            renameNode(node->stmt.ifn.if_body);
            if (node->stmt.ifn.else_body) {
                renameNode(node->stmt.ifn.else_body);
            }
            break;

        case ast_while:
            // While: rename condition and body
            renameNode(node->stmt.whilen.cond);
            renameNode(node->stmt.whilen.body);
            break;

        case ast_block:
            // Block: create a new scope for declarations in this block
            enterScope();
            renameStmtList(node);
            exitScope();
            break;

        default:
            break;
    }
}

/* Rename variables inside an AST node based on its node type. */
static void renameNode(astNode* node) {
    if (!node) return;

    switch (node->type) {
        case ast_prog:
            // Program: rename the function subtree
            renameNode(node->prog.func);
            break;

        case ast_func:
            // Function: new scope for parameter and body
            enterScope();

            // Parameter: treat as a declaration and rename it
            if (node->func.param && node->func.param->type == ast_var) {
                std::string unique = declareUnique(node->func.param->var.name);
                setAstName(node->func.param->var.name, unique);
            } else if (node->func.param) {
                renameNode(node->func.param);
            }

            // Body: rename all statements inside
            renameStmtList(node->func.body);

            exitScope();
            break;

        case ast_stmt:
            renameStatement(node);
            break;

        case ast_var: {
            // Variable use: replace name with its resolved unique name
            std::string unique = resolveUnique(node->var.name);
            if (!unique.empty()) {
                setAstName(node->var.name, unique);
            }
            break;
        }

        case ast_cnst:
            // Constant: nothing to rename
            break;

        case ast_uexpr:
            // Unary expression: rename child expression
            renameNode(node->uexpr.expr);
            break;

        case ast_bexpr:
            // Binary expression: rename lhs and rhs
            renameNode(node->bexpr.lhs);
            renameNode(node->bexpr.rhs);
            break;

        case ast_rexpr:
            // Relational expression: rename lhs and rhs
            renameNode(node->rexpr.lhs);
            renameNode(node->rexpr.rhs);
            break;

        case ast_call:
            // read() call in expression: no variable names inside
            break;

        case ast_extern:
            // extern: no variable names to rename
            break;

        default:
            break;
    }
}

/* Public entry point for the variable renaming pass. */
void RenameVariablesUnique(astNode* root) {
    // Reset state for a fresh rename pass
    scope_stack.clear();
    unique_id = 0;

    renameNode(root);

    scope_stack.clear();
}