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
