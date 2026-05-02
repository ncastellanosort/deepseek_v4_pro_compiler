#include "compiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *token_type_name(TokenType t) {
    switch(t){case TOK_EOF:return"EOF";case TOK_IDENT:return"ident";case TOK_NUMBER:return"núm";case TOK_STRING:return"string";case TOK_DEF:return"'def'";case TOK_IF:return"'if'";case TOK_ELSE:return"'else'";case TOK_WHILE:return"'while'";case TOK_FOR:return"'for'";case TOK_BREAK:return"'break'";case TOK_CONTINUE:return"'continue'";case TOK_RETURN:return"'return'";case TOK_PRINT:return"'print'";case TOK_ARRAY:return"'array'";case TOK_ASSIGN:return"=";case TOK_LPAREN:return"(";case TOK_RPAREN:return")";case TOK_LBRACE:return"{";case TOK_RBRACE:return"}";case TOK_LBRACK:return"[";case TOK_RBRACK:return"]";case TOK_SEMICOLON:return";";case TOK_COMMA:return",";case TOK_PLUS:return"+";case TOK_MINUS:return"-";case TOK_STAR:return"*";case TOK_SLASH:return"/";case TOK_MOD:return"%";case TOK_BITAND:return"&";case TOK_BITOR:return"|";case TOK_BITXOR:return"^";case TOK_NOT:return"!";case TOK_LSHIFT:return"<<";case TOK_RSHIFT:return">>";case TOK_EQ:return"==";case TOK_NE:return"!=";case TOK_LT:return"<";case TOK_GT:return">";case TOK_LE:return"<=";case TOK_GE:return">=";    case TOK_LAND:return"&&";case TOK_LOR:return"||";
    case TOK_PLUS_ASSIGN:return"+=";case TOK_MINUS_ASSIGN:return"-=";
    case TOK_STAR_ASSIGN:return"*=";case TOK_SLASH_ASSIGN:return"/=";
    case TOK_MOD_ASSIGN:return"%=";case TOK_AND_ASSIGN:return"&=";
    case TOK_OR_ASSIGN:return"|=";case TOK_XOR_ASSIGN:return"^=";
    case TOK_LS_ASSIGN:return"<<=";case TOK_RS_ASSIGN:return">>=";
    case TOK_INC:return"++";case TOK_DEC:return"--";
    case TOK_QUESTION:return"?";case TOK_COLON:return":";
    case TOK_DO:return"'do'";
    case TOK_INT:return"'int'";case TOK_CHAR:return"'char'";
    case TOK_SHORT:return"'short'";case TOK_LONG:return"'long'";
    case TOK_SWITCH:return"'switch'";case TOK_CASE:return"'case'";
    case TOK_DEFAULT:return"'default'";
    case TOK_CHAR_LITERAL:return"char";
    default:return"?";}
}

static ASTNode *alloc_node(ASTNodeType t){ASTNode*n=calloc(1,sizeof(ASTNode));if(!n){fprintf(stderr,"Error: sin memoria\n");exit(1);}n->type=t;return n;}

ASTNode *ast_make_program(ASTNode *f){ASTNode*n=alloc_node(AST_PROGRAM);n->next=f;return n;}
ASTNode *ast_make_block(ASTNode *f){ASTNode*n=alloc_node(AST_BLOCK);n->next=f;return n;}
ASTNode *ast_make_if(ASTNode*c,ASTNode*t){ASTNode*n=alloc_node(AST_IF);n->left=c;n->right=t;return n;}
ASTNode *ast_make_else(ASTNode*e){ASTNode*n=alloc_node(AST_ELSE);n->right=e;return n;}
ASTNode *ast_make_while(ASTNode*c,ASTNode*b){ASTNode*n=alloc_node(AST_WHILE);n->left=c;n->right=b;return n;}
ASTNode *ast_make_break(void){return alloc_node(AST_BREAK);}
ASTNode *ast_make_continue(void){return alloc_node(AST_CONTINUE);}

ASTNode *ast_make_func(const char *name,ASTNode*p,ASTNode*b){ASTNode*n=alloc_node(AST_FUNC);strncpy(n->var_name,name,MAX_LEXEME-1);n->var_name[MAX_LEXEME-1]='\0';n->left=p;n->right=b;return n;}
ASTNode *ast_make_param(const char *name){ASTNode*n=alloc_node(AST_PARAM);strncpy(n->var_name,name,MAX_LEXEME-1);n->var_name[MAX_LEXEME-1]='\0';return n;}
ASTNode *ast_make_return(ASTNode*e){ASTNode*n=alloc_node(AST_RETURN);n->left=e;return n;}
ASTNode *ast_make_call(const char *name,ASTNode*a){ASTNode*n=alloc_node(AST_CALL);strncpy(n->var_name,name,MAX_LEXEME-1);n->var_name[MAX_LEXEME-1]='\0';n->left=a;return n;}

ASTNode *ast_make_for(ASTNode *init,ASTNode *cond,ASTNode *step,ASTNode *body){
    ASTNode*n=alloc_node(AST_FOR);
    n->left=init;
    body->left=cond;
    if(step) ast_append_stmt(body, step);
    n->right=body;
    return n;
}

ASTNode *ast_make_assign(ASTNode*t,ASTNode*v){ASTNode*n=alloc_node(AST_ASSIGN);n->left=t;n->right=v;return n;}
ASTNode *ast_make_print(ASTNode*e){ASTNode*n=alloc_node(AST_PRINT);n->left=e;return n;}
ASTNode *ast_make_binary(char op,ASTNode*l,ASTNode*r){ASTNode*n=alloc_node(AST_BINARY);n->op=op;n->left=l;n->right=r;return n;}
ASTNode *ast_make_unary(char op,ASTNode*o){ASTNode*n=alloc_node(AST_UNARY);n->op=op;n->left=o;return n;}
ASTNode *ast_make_number(int v){ASTNode*n=alloc_node(AST_NUMBER);n->num_value=v;return n;}
ASTNode *ast_make_string(const char*s){ASTNode*n=alloc_node(AST_STRING);strncpy(n->var_name,s,MAX_LEXEME-1);n->var_name[MAX_LEXEME-1]='\0';return n;}
ASTNode *ast_make_variable(const char*nm){ASTNode*n=alloc_node(AST_VARIABLE);strncpy(n->var_name,nm,MAX_LEXEME-1);n->var_name[MAX_LEXEME-1]='\0';return n;}
ASTNode *ast_make_index(const char*arr,ASTNode*idx){ASTNode*n=alloc_node(AST_INDEX);strncpy(n->var_name,arr,MAX_LEXEME-1);n->var_name[MAX_LEXEME-1]='\0';n->left=idx;return n;}
ASTNode *ast_make_deref(ASTNode*p){ASTNode*n=alloc_node(AST_DEREF);n->left=p;return n;}
ASTNode *ast_make_addr(ASTNode*lv){ASTNode*n=alloc_node(AST_ADDR);n->left=lv;return n;}
ASTNode *ast_make_array_decl(const char*name,int size){ASTNode*n=alloc_node(AST_ARRAY_DECL);strncpy(n->var_name,name,MAX_LEXEME-1);n->var_name[MAX_LEXEME-1]='\0';n->num_value=size;return n;}
ASTNode *ast_make_ternary(ASTNode*cond,ASTNode*t,ASTNode*e){ASTNode*n=alloc_node(AST_TERNARY);n->left=cond;n->right=t;n->next=e;return n;}
ASTNode *ast_make_dowhile(ASTNode*body,ASTNode*cond){ASTNode*n=alloc_node(AST_DOWHILE);n->left=cond;n->right=body;return n;}
ASTNode *ast_make_decl(int type,const char*name,ASTNode*init){ASTNode*n=alloc_node(AST_DECL);n->num_value=type;strncpy(n->var_name,name,MAX_LEXEME-1);n->var_name[MAX_LEXEME-1]='\0';n->left=init;return n;}
ASTNode *ast_make_switch(ASTNode*expr,ASTNode*cases){ASTNode*n=alloc_node(AST_SWITCH);n->left=expr;n->right=cases;return n;}
ASTNode *ast_make_case(int value,ASTNode*body){ASTNode*n=alloc_node(AST_CASE);n->num_value=value;n->left=body;return n;}
ASTNode *ast_make_default(ASTNode*body){ASTNode*n=alloc_node(AST_DEFAULT);n->left=body;return n;}
ASTNode *ast_make_cast(int type,ASTNode*expr){ASTNode*n=alloc_node(AST_CAST);n->num_value=type;n->left=expr;return n;}

ASTNode *ast_clone(ASTNode*n){
    if(!n)return NULL;
    ASTNode*c=alloc_node(n->type);
    c->num_value=n->num_value;
    strncpy(c->var_name,n->var_name,MAX_LEXEME-1);c->var_name[MAX_LEXEME-1]='\0';
    c->op=n->op;
    c->left=ast_clone(n->left);
    c->right=ast_clone(n->right);
    c->next=ast_clone(n->next);
    return c;
}

void ast_append_stmt(ASTNode *list,ASTNode*s){if(!list->next){list->next=s;return;}ASTNode*t=list->next;while(t->next)t=t->next;t->next=s;}

void ast_free(ASTNode*n){if(!n)return;ast_free(n->left);ast_free(n->right);if(n->type==AST_PROGRAM||n->type==AST_BLOCK||n->type==AST_FUNC||n->type==AST_CASE||n->type==AST_DEFAULT){ASTNode*s=n->next;while(s){ASTNode*nx=s->next;s->next=NULL;ast_free(s);s=nx;}}free(n);}

static const char *op_name(char op){switch(op){case'+':return"+";case'-':return"-";case'*':return"*";case'/':return"/";case'%':return"%";case'&':return"&";case'|':return"|";case'^':return"^";case'<':return"<";case'>':return">";case OP_LSHIFT:return"<<";case OP_RSHIFT:return">>";case OP_EQ:return"==";case OP_NE:return"!=";case OP_LE:return"<=";case OP_GE:return">=";case OP_LAND:return"&&";case OP_LOR:return"||";case'm':return"NEG";case OP_NOT:return"NOT";case OP_DEREF:return"DEREF";case OP_ADDR:return"ADDR";default:return"?";}}

static void pi(int n){for(int i=0;i<n;i++)printf("  ");}
static void print_block(ASTNode*n,int ind){for(ASTNode*s=n->next;s;){if(s->type==AST_IF&&s->next&&s->next->type==AST_ELSE){ast_print(s,ind);s=s->next->next;}else if(s->type==AST_ELSE){pi(ind);printf("ELSE huérfano\n");s=s->next;}else{ast_print(s,ind);s=s->next;}}}

void ast_print(ASTNode*n,int ind){if(!n)return;pi(ind);switch(n->type){case AST_PROGRAM:printf("PROGRAM\n");print_block(n,ind+1);break;case AST_FUNC:printf("FUNC(%s)\n",n->var_name);pi(ind+1);printf("PARAMS:");if(!n->left)printf(" -\n");else{printf("\n");for(ASTNode*p=n->left;p;p=p->next){pi(ind+2);printf("PARAM(%s)\n",p->var_name);}}pi(ind+1);printf("BODY:\n");ast_print(n->right,ind+2);break;case AST_BLOCK:printf("BLOCK\n");print_block(n,ind+1);break;case AST_IF:printf("IF\n");pi(ind+1);printf("COND:\n");ast_print(n->left,ind+2);pi(ind+1);printf("THEN:\n");ast_print(n->right,ind+2);if(n->next&&n->next->type==AST_ELSE){pi(ind+1);printf("ELSE:\n");ast_print(n->next->right,ind+2);}break;case AST_WHILE:printf("WHILE\n");pi(ind+1);printf("COND:\n");ast_print(n->left,ind+2);pi(ind+1);printf("BODY:\n");ast_print(n->right,ind+2);break;case AST_FOR:printf("FOR\n");pi(ind+1);printf("INIT:\n");if(n->left)ast_print(n->left,ind+2);else{pi(ind+2);printf("-\n");}pi(ind+1);printf("COND:\n");if(n->right->left)ast_print(n->right->left,ind+2);else{pi(ind+2);printf("-\n");}pi(ind+1);printf("BODY:\n");ast_print(n->right,ind+2);break;case AST_RETURN:printf("RETURN\n");ast_print(n->left,ind+1);break;case AST_CALL:printf("CALL(%s)\n",n->var_name);for(ASTNode*a=n->left;a;a=a->next)ast_print(a,ind+1);break;case AST_ARRAY_DECL:printf("ARRAY_DECL(%s, %d)\n",n->var_name,n->num_value);break;case AST_BREAK:printf("BREAK\n");break;case AST_CONTINUE:printf("CONTINUE\n");break;case AST_ASSIGN:printf("ASSIGN\n");pi(ind+1);printf("TARGET:\n");ast_print(n->left,ind+2);pi(ind+1);printf("VALUE:\n");ast_print(n->right,ind+2);break;case AST_PRINT:printf("PRINT\n");ast_print(n->left,ind+1);break;case AST_BINARY:printf("BINARY(%s)\n",op_name(n->op));ast_print(n->left,ind+1);ast_print(n->right,ind+1);break;case AST_UNARY:printf("UNARY(%s)\n",op_name(n->op));ast_print(n->left,ind+1);break;case AST_NUMBER:printf("NUMBER(%d)\n",n->num_value);break;case AST_STRING:printf("STRING(\"%s\")\n",n->var_name);break;case AST_VARIABLE:printf("VARIABLE(%s)\n",n->var_name);break;case AST_INDEX:printf("INDEX(%s)\n",n->var_name);ast_print(n->left,ind+1);break;case AST_DEREF:printf("DEREF\n");ast_print(n->left,ind+1);break;case AST_ADDR:printf("ADDR\n");ast_print(n->left,ind+1);break;case AST_TERNARY:printf("TERNARY\n");pi(ind+1);printf("COND:\n");ast_print(n->left,ind+2);pi(ind+1);printf("THEN:\n");ast_print(n->right,ind+2);pi(ind+1);printf("ELSE:\n");ast_print(n->next,ind+2);break;case AST_DOWHILE:printf("DOWHILE\n");pi(ind+1);printf("BODY:\n");ast_print(n->right,ind+2);pi(ind+1);printf("COND:\n");ast_print(n->left,ind+2);break;case AST_DECL:{const char*t[]={"int","char","short","long"};printf("DECL(%s, %s)\n",t[n->num_value&3],n->var_name);if(n->left){pi(ind+1);printf("INIT:\n");ast_print(n->left,ind+2);}}break;case AST_SWITCH:printf("SWITCH\n");pi(ind+1);printf("EXPR:\n");ast_print(n->left,ind+2);pi(ind+1);printf("CASES:\n");if(n->right)ast_print(n->right,ind+2);else{pi(ind+2);printf("-\n");}break;case AST_CASE:printf("CASE(%d)\n",n->num_value);if(n->left)ast_print(n->left,ind+1);if(n->next)ast_print(n->next,ind);break;case AST_DEFAULT:printf("DEFAULT\n");if(n->left)ast_print(n->left,ind+1);break;case AST_CAST:{const char*t[]={"int","char","short","long"};printf("CAST(%s)\n",t[n->num_value&3]);ast_print(n->left,ind+1);}break;default:printf("???\n");break;}}
