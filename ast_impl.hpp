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


