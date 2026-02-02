#include "semantic.h"

#include <cstdio>
#include <string>
#include <unordered_set>
#include <vector>

static std::vector<std::unordered_set<std::string> > scope_stack;
static int error_count = 0;

/* Report a duplicate declaration error. */
static void reportDuplicate(const char *name) {
    std::fprintf(stderr, "Semantic error: duplicate declaration of '%s'\n", name);
    ++error_count;
}

/* Report an undeclared variable error. */
static void reportUndeclared(const char *name) {
    std::fprintf(stderr, "Semantic error: undeclared variable '%s'\n", name);
    ++error_count;
}

/* Push a new empty scope. */
static void enterScope() {
    scope_stack.push_back(std::unordered_set<std::string>());
}

/* Pop the current scope. */
static void exitScope() {
    if (!scope_stack.empty()) {
        scope_stack.pop_back();
    }
}


/* Declare a name in the current scope. */
static void declareName(const char *name) {
    if (name == nullptr) {
        return;
    }
    if (scope_stack.empty()) {
        enterScope();
    }
    std::unordered_set<std::string> &current = scope_stack.back();
    if (current.find(name) != current.end()) {
        reportDuplicate(name);
        return;
    }
    current.insert(name);
}

/* Check that a name is declared in some scope. */
static void useName(const char *name) {
    int i;

    if (name == nullptr) {
        return;
    }
    if (scope_stack.empty()) {
        reportUndeclared(name);
        return;
    }
    for (i = (int)scope_stack.size() - 1; i >= 0; --i) {
        if (scope_stack[i].find(name) != scope_stack[i].end()) {
            return;
        }
    }
    reportUndeclared(name);
}

static void checkNode(astNode *node);

/* Walk all statements inside a block. */
static void checkBlockStatements(astNode *node) {
    std::vector<astNode *> *list_ptr;
    size_t i;

    if (node == nullptr) {
        return;
    }
    if (node->type != ast_stmt || node->stmt.type != ast_block) {
        checkNode(node);
        return;
    }
    list_ptr = node->stmt.block.stmt_list;
    if (list_ptr == nullptr) {
        return;
    }
    for (i = 0; i < list_ptr->size(); ++i) {
        checkNode((*list_ptr)[i]);
    }
}

/* Check a statement node and its children. */
static void checkStatement(astNode *node) {
    if (node == nullptr || node->type != ast_stmt) {
        return;
    }
    switch (node->stmt.type) {
    case ast_call:
        if (node->stmt.call.param != nullptr) {
            checkNode(node->stmt.call.param);
        }
        break;
    case ast_ret:
        checkNode(node->stmt.ret.expr);
        break;
    case ast_block:
        enterScope();
        checkBlockStatements(node);
        exitScope();
        break;
    case ast_while:
        checkNode(node->stmt.whilen.cond);
        checkNode(node->stmt.whilen.body);
        break;
    case ast_if:
        checkNode(node->stmt.ifn.cond);
        checkNode(node->stmt.ifn.if_body);
        if (node->stmt.ifn.else_body != nullptr) {
            checkNode(node->stmt.ifn.else_body);
        }
        break;
    case ast_asgn:
        checkNode(node->stmt.asgn.lhs);
        checkNode(node->stmt.asgn.rhs);
        break;
    case ast_decl:
        declareName(node->stmt.decl.name);
        break;
    default:
        break;
    }
}

/* Traverse an AST node */
static void checkNode(astNode *node) {
    if (node == nullptr) {
        return;
    }
    switch (node->type) {
    case ast_prog:
        checkNode(node->prog.func);
        break;
    case ast_func:
        enterScope();
        if (node->func.param != nullptr && node->func.param->type == ast_var) {
            declareName(node->func.param->var.name);
        } else if (node->func.param != nullptr) {
            checkNode(node->func.param);
        }
        checkBlockStatements(node->func.body);
        exitScope();
        break;
    case ast_stmt:
        checkStatement(node);
        break;
    case ast_var:
        useName(node->var.name);
        break;
    case ast_cnst:
        break;
    case ast_rexpr:
        checkNode(node->rexpr.lhs);
        checkNode(node->rexpr.rhs);
        break;
    case ast_bexpr:
        checkNode(node->bexpr.lhs);
        checkNode(node->bexpr.rhs);
        break;
    case ast_uexpr:
        checkNode(node->uexpr.expr);
        break;
    case ast_extern:
        break;
    default:
        break;
    }
}

int SemanticAnalysis(astNode *root) {
    scope_stack.clear();
    error_count = 0;
    checkNode(root);
    return (error_count > 0) ? 1 : 0;
}


