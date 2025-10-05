// AST structure (struct ConfyFile is opaque)
struct ConfyFile;
struct ConfyState;

struct SyntaxNode {
    virtual std::string Render(ConfyFile *f, ConfyState *st) =0;
    virtual ConfyVal Execute(ConfyFile *f, ConfyState *st, bool enable) =0;
};

struct Seq : public SyntaxNode {
    std::vector<SyntaxNode*> children;

    virtual std::string Render(ConfyFile *f, ConfyState *st);
    virtual ConfyVal Execute(ConfyFile *f, ConfyState *st, bool enable);
};


struct SourceBlock : public SyntaxNode {
    enum {
        B_ACTIVE,
        B_INERT_LINE,
        B_INERT_BLOCK,
        B_META_CHAFF
    } bType;
    std::string contents;

    virtual std::string Render(ConfyFile *f, ConfyState *st);
    virtual ConfyVal Execute(ConfyFile *f, ConfyState *st, bool enable);
};

struct IfThen : public SyntaxNode {
    std::string pre, post;

    SyntaxNode *cond, *sub;

    virtual std::string Render(ConfyFile *f, ConfyState *st);  
    virtual ConfyVal Execute(ConfyFile *f, ConfyState *st, bool enable);
};

struct IfThenElse : public SyntaxNode {
    std::string pre, inter, post;

    SyntaxNode *cond, *sub1, *sub2;

    virtual std::string Render(ConfyFile *f, ConfyState *st);
    virtual ConfyVal Execute(ConfyFile *f, ConfyState *st, bool enable);
};

struct Template : public SyntaxNode {
    std::string pre, inter, post;
    
    SyntaxNode *temp;
    std::string out;

    virtual std::string Render(ConfyFile *f, ConfyState *st);
    virtual ConfyVal Execute(ConfyFile *f, ConfyState *st, bool enable);
};

struct Expr {
    virtual ConfyVal Eval(ConfyFile *f, ConfyState *st) = 0;
};
struct ExprVar : public Expr {
    std::string name;
    virtual ConfyVal Eval(ConfyFile *f, ConfyState *st);
};
struct ExprLiteral : public Expr {
    ConfyVal v;
    virtual ConfyVal Eval(ConfyFile *f, ConfyState *st);
};
struct ExprOp : public Expr {
    std::function<ConfyVal (ConfyVal,ConfyVal,int)> op;
    std::vector<Expr*> subexprs;
    std::vector<int> subtypes;
    virtual ConfyVal Eval(ConfyFile *f, ConfyState *st);
};
struct ExprNot : public Expr {
    Expr *sub;
    virtual ConfyVal Eval(ConfyFile *f, ConfyState *st);   
};
struct ExprNeg : public Expr {
    Expr *sub;
    virtual ConfyVal Eval(ConfyFile *f, ConfyState *st);   
};

struct ExprEq : public Expr {
    Expr *left, *right;
    virtual ConfyVal Eval(ConfyFile *f, ConfyState *st);
};

struct ExprNode : public SyntaxNode {
    Expr *root;
    std::string source;

    virtual std::string Render(ConfyFile *f, ConfyState *st);
    virtual ConfyVal Execute(ConfyFile *f, ConfyState *st, bool enable);
};

struct VarDef : public SyntaxNode {
    std::string pre, post;

    std::string name;
    bool hidden;    

    ConfyVar v;

    virtual std::string Render(ConfyFile *f, ConfyState *st);
    virtual ConfyVal Execute(ConfyFile *f, ConfyState *st, bool enable);
};

struct VarAssign : public SyntaxNode {
    std::string source;

    std::string varname;
    SyntaxNode *expr;

    virtual std::string Render(ConfyFile *f, ConfyState *st);
    virtual ConfyVal Execute(ConfyFile *f, ConfyState *st, bool enable);
};

