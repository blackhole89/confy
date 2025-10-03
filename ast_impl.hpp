// AST functions implementations

std::string Seq::Render(ConfyFile *f, ConfyState *st) {
    std::string ret;
    for(auto n : children) 
        ret += n->Render(f, st);
    return ret;
}

ConfyVal Seq::Execute(ConfyFile *f, ConfyState *st, bool enable) {
    ConfyVal ret;
    for(auto n : children)
        ret = n->Execute(f,st,enable);
    return ret;
}

std::string VarDef::Render(ConfyFile *f, ConfyState *st) {
    std::string ret;
    ret += pre;
    ret += v.val.Render();
    ret += post;
    return ret;
}

ConfyVal VarDef::Execute(ConfyFile *f, ConfyState *st, bool enable) {
    if(!st->vars.count(name)) {
        st->vars[name] = v;
        st->vars[name].fl = f;
    }
    st->vars[name].hidden = !enable;
    return ConfyVal { T_BOOL, true, 1, 1.0, "true" };
}

std::string SourceBlock::Render(ConfyFile *f, ConfyState *st) {
    std::string ret;
    if(bType == B_INERT_LINE) ret += f->setup.line;
    if(bType == B_INERT_BLOCK) ret += f->setup.block_start;
    ret += contents;
    if(bType == B_INERT_BLOCK) ret += f->setup.block_end;
    return ret;
}

ConfyVal SourceBlock::Execute(ConfyFile *f, ConfyState *st, bool enable) {
    if(enable && bType!=B_META_CHAFF) bType=B_ACTIVE;
    else if(bType!=B_META_CHAFF) bType=B_INERT_BLOCK;

    return ConfyVal { T_BOOL, true, 1, 1.0, "true" };
}

std::string IfThen::Render(ConfyFile *f, ConfyState *st) {
    std::string ret;
    ret += pre;
    ret += sub->Render(f,st);
    ret += post;
    return ret;
}

ConfyVal IfThen::Execute(ConfyFile *f, ConfyState *st, bool enable) {
    ConfyVal v { T_BOOL, false, 0, 0.0, "false" };
    if(!enable) sub->Execute(f,st,false);
    else {
        v = cond->Execute(f,st,true);
        sub->Execute(f,st,v.b);
    }
    return v; 
}

std::string IfThenElse::Render(ConfyFile *f, ConfyState *st) {
    std::string ret;
    ret += pre;
    ret += sub1->Render(f,st);
    ret += inter;
    ret += sub2->Render(f,st);
    ret += post;
    return ret;
}

ConfyVal IfThenElse::Execute(ConfyFile *f, ConfyState *st, bool enable) {
    ConfyVal v { T_BOOL, false, 0, 0.0, "false" };
    if(!enable) { sub1->Execute(f,st,false); sub2->Execute(f,st,false); }
    else {
        v = cond->Execute(f,st,true);
        sub1->Execute(f,st,v.b);
        sub2->Execute(f,st,!v.b);
    }
    return v; 
}

std::string ExprNode::Render(ConfyFile *f, ConfyState *st) {
    return source;
}

ConfyVal ExprNode::Execute(ConfyFile *f, ConfyState *st, bool enable) {
    return root->Eval(f,st);
}

ConfyVal ExprVar::Eval(ConfyFile *f, ConfyState *st) {
    if(!st->vars.count(name)) return ConfyVal { T_BOOL, false, 0, 0.0, "false" };
    return st->vars[name].val;
}
ConfyVal ExprLiteral::Eval(ConfyFile *f, ConfyState *st) {
    return v;
}
// check for equality, coercing to type of left
ConfyVal ExprEq::Eval(ConfyFile *f, ConfyState *st) {
    ConfyVal l = left->Eval(f,st);
    ConfyVal r = right->Eval(f,st);
    bool res;
    switch(l.t) {
    case T_BOOL: res = l.b == r.b; break;
    case T_INT: res = l.i == r.i; break;
    case T_FLOAT: res = l.f == r.f; break;
    case T_STRING: res = l.s == r.s; break;
    default: res = false;
    }
    return ConfyVal { T_BOOL, res, res?1:0, res?1.0:0.0, res?"true":"false" };
}

