#include <stdio.h>
#include <stdbool.h>

#include "ast_printer.h"
#include "util.h"
#include "table.h"
#include "typecheck.h"

static T_type INTTYPE;
static T_type CHARTYPE;
static T_type STRINGTYPE;
static T_type STRINGARRAYTYPE;

static T_scope current_scope;

static T_scope create_scope(T_scope parent) {
  T_scope scope = xmalloc(sizeof(*scope));
  scope->table = create_table();
  scope->parent = parent;
  return scope;
}

static void destroy_scope(T_scope scope) {
  free(scope);
}

static T_type lookup_in_all_scopes(T_scope scope, string ident) {
  // loop over each scope and check for a binding for ident
  while (NULL != scope) {
    // check for a binding in this scope
    T_type type = (T_type) lookup(scope->table, ident);
    if (NULL != type) {
      return type;
    }
    // no binding in this scope, so check the parent
    scope = scope->parent;
  }
  // found no binding in any scope
  return NULL;
}

/* the root of the AST */
void check_prog(T_prog prog) {
  // initialize useful types
  INTTYPE = create_primitivetype(E_typename_int);
  CHARTYPE = create_primitivetype(E_typename_char);
  STRINGTYPE = create_pointertype(CHARTYPE);
  STRINGARRAYTYPE = create_pointertype(STRINGTYPE);
  // create the global scope
  current_scope = create_scope(NULL);
  // add the global declarations
  check_decllist(prog->decllist);
  // check the function definitions
  check_funclist(prog->funclist);
  // check the main function
  check_main(prog->main);
  // clean-up the global scope
  destroy_scope(current_scope);
}

/* declarations and function definitions */
static void check_decllist(T_decllist decllist) {
  while (NULL != decllist) {
    check_decl(decllist->decl);
    decllist = decllist->tail;
  }
}

static void check_decl(T_decl decl)
{
    if( NULL != lookup( current_scope->table, decl->ident ) )
    {
      type_error( "duplicate declaration of same symbol " );
    }

    insert( current_scope->table, decl->ident, decl->type );
}

static void check_funclist(T_funclist funclist) {
  while (NULL != funclist) {
    check_func(funclist->func);
    funclist = funclist->tail;
  }
}

static void check_func(T_func func)
{
    T_paramlist parameterList;
    T_typelist parameterTypes;
    
    fprintf( stderr, "check_func\n" );

    if( func->type->kind != E_functiontype )
    {
        type_error("not declared as a function" );
    }
    if( lookup( current_scope->table, func->ident ) != NULL )
    {
        type_error("function is already defined in current scope" );
    }

    insert( current_scope->table, func->ident, func->type );

    current_scope = create_scope( current_scope );

    parameterList = func->paramlist;
    parameterTypes = func->type->functiontype.paramtypes;

    while( parameterList != NULL && parameterTypes != NULL )
    {
        insert( current_scope->table, parameterList->ident, parameterTypes->type );

        parameterTypes = parameterTypes->tail;
        parameterList = parameterList->tail;
    }

    if( parameterList != NULL || parameterTypes != NULL )
    {
        type_error("the number of parameters must match the number of function parameter types");
    }

    check_decllist( func->decllist );
    check_stmtlist( func->stmtlist );
    check_expr( func->returnexpr );

    if( ! compare_types( func->type->functiontype.returntype, func->returnexpr->type ) )
    {
        type_error("function return type does not match return expression type" );
    }

    T_scope parent_scope = current_scope->parent;

    destroy_scope( current_scope );
    current_scope = parent_scope;
    

    
}

// GIVEN
static void check_main(T_main main) {
  fprintf(stderr, "check_main");
  // create a new scope
  current_scope = create_scope(current_scope);
  // add argc and argv with their C runtime types
  insert(current_scope->table, "argc", INTTYPE);
  insert(current_scope->table, "argv", STRINGARRAYTYPE);
  // add the declarations to the symtab
  check_decllist(main->decllist);
  // check the function body for type errors
  check_stmtlist(main->stmtlist);
  // check the return expression for type errors
  check_expr(main->returnexpr);
  // check that the return type is an int, per C runtime
  if (! compare_types(main->returnexpr->type, INTTYPE)) {
    type_error("the return expression type does not match the function type");
  }
  // restore the parent symbol table
  T_scope parent_scope = current_scope->parent;
  destroy_scope(current_scope); current_scope = parent_scope;
}

/* statements */
static void check_stmtlist(T_stmtlist stmtlist) {
  while (NULL != stmtlist) {
    check_stmt(stmtlist->stmt);
    stmtlist = stmtlist->tail;
  }
}

static void check_stmt(T_stmt stmt) {
  if (NULL == stmt) {
    fprintf(stderr, "FATAL: stmt is NULL in check_stmt\n");
    exit(1);
  }
  switch (stmt->kind) {
  case E_assignstmt: check_assignstmt(stmt); break;
  case E_ifstmt: check_ifstmt(stmt); break;
  case E_ifelsestmt: check_ifelsestmt(stmt); break;
  case E_whilestmt: check_whilestmt(stmt); break;
  case E_compoundstmt: check_compoundstmt(stmt); break;
  default: fprintf(stderr, "FATAL: unexpected stmt kind in check_stmt\n"); exit(1); break;
  }
}

static void check_assignstmt(T_stmt stmt) {
  // check the type of the left-hand-side
  check_expr(stmt->assignstmt.left);
  // check the type of the right-hand-side
  check_expr(stmt->assignstmt.right);
  // check that the left-hand-side is an l-value, i.e., an identexpr or a deref unary expression
  switch (stmt->assignstmt.left->kind) {
  case E_identexpr:
    // okay
    break;
  case E_unaryexpr:
    switch (stmt->assignstmt.left->unaryexpr.op) {
    case E_op_deref:
      // okay
      break;
    default:
      type_error("assignment is not to an l-value");
      break;
    }
    break;
  case E_arrayexpr:
    // okay
    break;
  default:
    type_error("assignment is not to an l-value");
    break;
  }
  // check that the types of the left- and right-hand sides match
  if (! compare_types(stmt->assignstmt.left->type, stmt->assignstmt.right->type)) {
    type_error("left- and right-hand sides of the assignstmt have mismatched types");
  }
}

static void check_ifstmt(T_stmt stmt) {
  // check the condition expression
  check_expr(stmt->ifstmt.cond);
  // check that the condition is an int
  if (! compare_types(stmt->ifstmt.cond->type, INTTYPE)) {
    type_error("if condition must be an int");
  }
  // recursively check the if branch
  check_stmt(stmt->ifstmt.body);
}

static void check_ifelsestmt(T_stmt stmt)
{
    check_expr( stmt->ifelsestmt.cond );

    if( ! compare_types( stmt->ifelsestmt.cond->type, INTTYPE ) )
    {
        type_error("if condition must be an int\n" );
    }

    check_stmt( stmt->ifelsestmt.ifbranch );
    check_stmt( stmt->ifelsestmt.elsebranch );

}

static void check_whilestmt( T_stmt stmt )
{
    check_expr( stmt->whilestmt.cond );

    if( ! compare_types( stmt->whilestmt.cond->type, INTTYPE ) )
    {
        type_error("while statement condition must be type INT\n" );
    }

    check_stmt( stmt->whilestmt.body );
}

static void check_compoundstmt(T_stmt stmt)
{
    current_scope = create_scope( current_scope );

    check_stmtlist( stmt->compoundstmt.stmtlist );
    check_decllist( stmt->compoundstmt.decllist );

    T_scope parent_scope = current_scope->parent;
    destroy_scope( current_scope );

    current_scope = parent_scope;

}

/* expressions */
static void check_expr(T_expr expr) {
  if (NULL == expr) {
    fprintf(stderr, "FATAL: unexpected NULL in check_expr\n");
    exit(1);
  }
  switch (expr->kind) {
  case E_identexpr: check_identexpr(expr); break;
  case E_callexpr: check_callexpr(expr); break;
  case E_intexpr: check_intexpr(expr); break;
  case E_charexpr: check_charexpr(expr); break;
  case E_strexpr: check_strexpr(expr); break;
  case E_arrayexpr: check_arrayexpr(expr); break;
  case E_unaryexpr: check_unaryexpr(expr); break;
  case E_binaryexpr: check_binaryexpr(expr); break;
  case E_castexpr: check_castexpr(expr); break;
  default: fprintf(stderr, "FATAL: unexpected expr kind in check-expr\n"); exit(1); break;
  }
}

static void check_identexpr(T_expr expr)
{
    fprintf( stderr, "check_identexpre\n" );

    T_type binding = lookup_in_all_scopes( current_scope, expr->identexpr );

    if( NULL == binding )
    {
      type_error( "use of undeclared symbol in identexpr\n" );
    }
    if( E_functiontype == binding->kind )
    {
      type_error( "cannot use a function as a value " );
    }

    expr->type = binding;
}

static void check_callexpr(T_expr expr)
{
    T_exprlist arguments;
    T_typelist parameterTypes;

    fprintf( stderr, "check_callexpr\n" );

    T_type binding = lookup_in_all_scopes( current_scope, expr->callexpr.ident );

    if( NULL == binding )
    {
      type_error("function call to an undefined symbol\n" );
    }
    if( E_functiontype != binding->kind )
    {
      type_error("function call to symbol not declared as a function\n" );
    }

    arguments = expr->callexpr.args;
    parameterTypes = binding->functiontype.paramtypes;

    while( NULL != arguments && NULL != parameterTypes )
    {
        check_expr( arguments->expr );

        if( !compare_types( arguments->expr->type, parameterTypes->type ) )
        {
            type_error("type mismatch in function call" );
        }

        arguments = arguments->tail;
        parameterTypes = parameterTypes->tail;
    }

    if( NULL != arguments || NULL != parameterTypes )
    {
        type_error("the number of arguments of the function call does not match the function type" );
    }

    expr->type = binding->functiontype.returntype;

}

static void check_intexpr(T_expr expr)
{
    fprintf( stderr, "check_intexpr\n" );

    expr->type = INTTYPE;
}

static void check_charexpr(T_expr expr)
{
  fprintf( stderr, "check_charexpr\n" );

  expr->type = CHARTYPE;

}

static void check_strexpr(T_expr expr)
{
  fprintf( stderr, "check_strexpr\n" );

  expr->type = STRINGTYPE;

}

static void check_arrayexpr(T_expr expr)
{
    fprintf( stderr, "check_arrayexpr\n" );

    check_expr( expr->arrayexpr.expr );

    switch( expr->arrayexpr.expr->type->kind )
    {
      case E_arraytype:
          fprintf( stderr, "array type\n" );
          expr->type = expr->arrayexpr.expr->type->arraytype.type;
          break;
      case E_pointertype:
          fprintf( stderr, "pointer type\n" );
          expr->type = expr->arrayexpr.expr->type->pointertype;
          break;
      default:
          type_error("invalid array type");
          exit(1);
    }

}

static void check_unaryexpr(T_expr expr)
{
    fprintf( stderr, "check_unaryexpr\n" );

    check_expr( expr->unaryexpr.expr );

    if( expr->unaryexpr.op == E_op_ref )
    {
      expr->type = create_pointertype( expr->unaryexpr.expr->type );
    }
    else if( expr->unaryexpr.op == E_op_deref )
    {
        if( expr->unaryexpr.expr->type->kind != E_pointertype )
        {
          type_error("non pointer error\n" );
        }
        expr->type = expr->unaryexpr.expr->type->pointertype;
    }
    else if( expr->unaryexpr.op == E_op_minus )
    {
        expr->type = expr->unaryexpr.expr->type;
    }
    else if( expr->unaryexpr.op == E_op_not )
    {
        expr->type = INTTYPE;
    }
    else
    {
        fprintf( stderr, "unexpected operator\n" );
        exit(1);
    }

}

static void check_binaryexpr(T_expr expr)
{
    fprintf( stderr, "check_binaryexpr\n" );

    check_expr( expr->binaryexpr.left );
    check_expr( expr->binaryexpr.right );

    if( ! compare_types( expr->binaryexpr.left->type, expr->binaryexpr.right->type ) )
    {
      type_error( "types do not match" );
    }

    switch( expr->binaryexpr.op )
    {
        case E_op_times:
        case E_op_divide:
        case E_op_mod:
        case E_op_minus:
        case E_op_plus:
            expr->type = expr->binaryexpr.left->type;
            break;
        case E_op_lt:
        case E_op_gt:
        case E_op_le:
        case E_op_ge:
        case E_op_eq:
        case E_op_ne:
            expr->type = INTTYPE;
            break;
        case E_op_and:
        case E_op_or:
            if( ! compare_types( expr->binaryexpr.left->type, INTTYPE ) )
            {
              type_error("and/or operators must take 2 integers\n");
            }

            expr->type = INTTYPE;
            break;
        default:
            type_error( "unknown operator in binary expression\n" );
            exit(1);

    }
}

static void check_castexpr(T_expr expr)
{
    check_expr( expr->castexpr.expr );

    switch( expr->castexpr.expr->type->kind )
    {
        case E_primitivetype:
        case E_pointertype:
            expr->type = expr->castexpr.type;
            break;
        case E_functiontype:
            type_error("cannot type cast a function type" );
        default:
            type_error( "not a valid type casting type" );
    }
}

/* type error */
static void type_error(char *msg) {
  fprintf(stderr, "%s\n", msg);
  exit(3);
}

/* type comparison */
bool compare_types(T_type type1, T_type type2) {
  if (NULL == type1 || NULL == type2) {
    fprintf(stderr, "FATAL: unexpected NULL values in compare_types\n");
    exit(1);
  }
  if (type1->kind == type2->kind) {
    switch (type1->kind) {
    case E_primitivetype:
      // the primitive type names should match
      return type1->primitivetype == type2->primitivetype;
      break;
    case E_pointertype:
      // the types the pointers point to should match
      return compare_types(type1->pointertype, type2->pointertype);
      break;
    case E_arraytype:
      // both the size and the type should match
      return type1->arraytype.size == type2->arraytype.size
        && compare_types(type1->arraytype.type, type2->arraytype.type);
      break;
    case E_functiontype:
      {
        // the parameter types, their number, and the return type should match
        T_typelist params1 = type1->functiontype.paramtypes;
        T_typelist params2 = type2->functiontype.paramtypes;
        while (NULL != params1 && NULL != params2) {
          if (! compare_types(params1->type, params2->type)) {
            // the parameter types do not match
            return false;
          }
          params1 = params1->tail;
          params2 = params2->tail;
        }
        if (NULL != params1 || NULL != params2) {
          // the number of parameters do not match
          return false;
        }
        return compare_types(type1->functiontype.returntype, type2->functiontype.returntype);
      }
      break;
    default:
      fprintf(stderr, "FATAL: unexpected kind in compare_types\n");
      exit(1);
    }
  } else {
    return false;
  }
}
