/*
 *  File Name: preprocessor.h
 *  Description: This file declares the variable renaming preprocessor for an AST
 *  Author: Papa Yaw Owusu Nti
 */

#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#include "ast.h"

/*
 * Walks the entire AST starting from the root node.
 * Every declared variable (including parameters) is given
 * a new unique name. All uses of those variables are updated to match
 * the new unique names.
 */
void RenameVariablesUnique(astNode *root);

#endif