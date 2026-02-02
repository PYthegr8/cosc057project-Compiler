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