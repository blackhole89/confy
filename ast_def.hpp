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

struct VarDef : public SyntaxNode {
    std::string pre, post;

    std::string name;

    ConfyVar v;

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

