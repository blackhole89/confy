// interactive mode impl

#define TB_IMPL
#include "termbox2/termbox2.h"

#define STB_TEXTEDIT_CHARTYPE uint32_t
#define STB_TEXTEDIT_POSITIONTYPE int
#define STB_TEXTEDIT_UNDOSTATECOUNT 0
#define STB_TEXTEDIT_UNDOCHARCOUNT 0
#include "stb_textedit.h"
#define STB_TEXTEDIT_IMPLEMENTATION
#define STB_TEXTEDIT_STRING std::vector<uint32_t>
#define STB_TEXTEDIT_STRINGLEN(obj) obj->size()
static void    STB_TEXTEDIT_LAYOUTROW(StbTexteditRow* r, std::vector<uint32_t> *chars, int pos)
{
    r->x0 = 0.0f;
    r->x1 = chars->size()-pos+1;
    r->baseline_y_delta = 1;
    r->ymin = 0.0f;
    r->ymax = 1.0f;
    r->num_chars = chars->size()-pos;
}
#define STB_TEXTEDIT_GETWIDTH(obj,n,i) 1
#define STB_TEXTEDIT_GETCHAR(obj,i) (*obj)[i]
#define STB_TEXTEDIT_NEWLINE ((uint32_t)'\n')
#define STB_TEXTEDIT_DELETECHARS(obj,i,n) obj->erase(obj->begin()+i, obj->begin()+(i+n))
#define STB_TEXTEDIT_INSERTCHARS(obj,i,c,n) obj->insert(obj->begin()+i, c, c+n),true
#define STB_TEXTEDIT_K_SHIFT 0x1000000
#define STB_TEXTEDIT_K_LEFT TB_KEY_ARROW_LEFT
#define STB_TEXTEDIT_K_RIGHT TB_KEY_ARROW_RIGHT
#define STB_TEXTEDIT_K_UP TB_KEY_ARROW_UP
#define STB_TEXTEDIT_K_DOWN TB_KEY_ARROW_DOWN
#define STB_TEXTEDIT_K_PGUP TB_KEY_PGUP     
#define STB_TEXTEDIT_K_PGDOWN TB_KEY_PGDN 
#define STB_TEXTEDIT_K_LINESTART TB_KEY_HOME
#define STB_TEXTEDIT_K_LINEEND TB_KEY_END   
#define STB_TEXTEDIT_K_TEXTSTART TB_KEY_HOME|0x2000000
#define STB_TEXTEDIT_K_TEXTEND TB_KEY_END|0x2000000
#define STB_TEXTEDIT_K_DELETE TB_KEY_DELETE      
#define STB_TEXTEDIT_K_BACKSPACE TB_KEY_BACKSPACE2
#define STB_TEXTEDIT_K_UNDO TB_KEY_CTRL_Z
#define STB_TEXTEDIT_K_REDO TB_KEY_CTRL_Y
#include "stb_textedit.h"

std::string u32tou8(std::vector<uint32_t> *source)
{
    std::string ret;
    char buf[7];
    for(auto c : *source) ret+=std::string(buf, tb_utf8_unicode_to_char(buf, c));
    return ret;
}
void u8tou32(std::vector<uint32_t> *target, std::string source)
{
    int pos=0;
    target->clear();
    while(pos<source.length()) {
        uint32_t c;
        pos += tb_utf8_char_to_unicode(&c, source.c_str()+pos);
        target->push_back(c);
    }
}

void interact(ConfyState &st)
{
    struct tb_event ev;
    int y = 0;

    // get max width of all displayed names
    int maxw = 1;
    for(auto &n : st.varNames) {
        auto &nd = st.vars[n].display;
        if(nd.length() > maxw) maxw = nd.length();
    }

    tb_init();

    int sel=0, scroll=0;
    bool editing=false;

    STB_TexteditState test;
    std::vector<uint32_t> tecontents;

    while(1) {
        int h = tb_height(), w = tb_width();
        if(!editing) tb_hide_cursor();

        tb_clear();

        int i, ri = scroll; // running index to render
        int sel_y=0; // computed y-position of selection in list
        std::string statusline; // status line to emit at bottom
        for(i=0;i<h-4;++i) {
            while(ri < st.varNames.size() && st.vars[st.varNames[ri]].hidden) ++ri;
            if(ri==st.varNames.size()) break;

            ConfyVar &v = st.vars[st.varNames[ri]];

            bool selected = false;
            if(sel == ri) {
                selected=true;
                sel_y=i;

                statusline = st.files[v.fl].fname;
                statusline+=": ";
                switch(v.val.t) {
                case T_BOOL: statusline+="bool"; break;
                case T_INT: statusline+="int"; break;
                case T_FLOAT: statusline+="float"; break;
                case T_STRING: statusline+="string"; break;
                }
                statusline+=" $";
                statusline+=st.varNames[ri];
            }

            int bg,bghi,fg,fghi;
            bg=TB_DEFAULT; bghi=TB_DEFAULT;
            if(selected) {
                fg = TB_REVERSE|TB_DEFAULT|TB_DIM; 
                fghi = TB_REVERSE|TB_DEFAULT|TB_DIM;
            } else {
                fg = TB_DEFAULT|TB_DIM;      
                fghi = TB_DEFAULT|TB_BRIGHT; 
            }

            tb_printf(1, i+1, fg, bg, "     %s ", v.display.c_str());
            if(v.val.t == T_BOOL) {
                tb_printf(1, i+1, fghi, bghi, " [ ] ");
                if(v.val.b)
                    tb_printf(3, i+1, fg, bg, "X");
            } else { //if(v.val.t == T_INT) {
                if(!editing || !selected) {
                    tb_printf(5 + maxw + 3, i+1, TB_DEFAULT, 0, "%s", v.val.s.c_str());
                } else {
                    //tb_printf(5 + maxw + 3, i+1, TB_DEFAULT, 0, "%100s", " ");
                    for(int j=0;j<tecontents.size();++j) {
                        if(  (j >= test.select_start && j < test.select_end)
                           ||(j >= test.select_end && j < test.select_start)) bg=TB_BLUE|TB_DIM;
                        else bg=TB_DEFAULT;
                        tb_set_cell(5 + maxw + 3 +j, i+1, tecontents[j], TB_DEFAULT, bg);
                    }
                    tb_set_cursor(5 + maxw + 3 + test.cursor, i+1);
                }
            }

            ++ri;
        }
        tb_printf(0, i+2, TB_DIM, 0, "%s", statusline.c_str());
        tb_printf(0, i+3, TB_DIM|TB_REVERSE, 0, "Ctrl+C");
        tb_printf(7, i+3, TB_DIM|TB_DEFAULT, 0, "SaveQuit");
        tb_printf(17, i+3, TB_DIM|TB_REVERSE, 0, "Ctrl+D");
        tb_printf(24, i+3, TB_DIM|TB_DEFAULT, 0, "Abort");


        tb_present();

        tb_poll_event(&ev);

        if (ev.type == TB_EVENT_KEY) {
            int sel0;
            switch(ev.key) {
            case TB_KEY_ARROW_UP:
                sel0=sel;
                if(sel > 0) --sel;
                while(sel > 0 && st.vars[st.varNames[sel]].hidden) --sel;
                if(st.vars[st.varNames[sel]].hidden) sel=sel0; // bumped into end on hidden, revert
                // check if we also need to scroll up
                if(sel_y<2) {
                    if(scroll>0) --scroll;
                    while(scroll > 0 && st.vars[st.varNames[scroll]].hidden) --scroll;
                    // allow scrolling up to hidden
                }
                editing=false;
                break;
            case TB_KEY_ARROW_DOWN:
                sel0=sel;
                if(sel < (st.varNames.size()-1)) ++sel;
                while(sel < (st.varNames.size()-1) && st.vars[st.varNames[sel]].hidden) ++sel;
                if(st.vars[st.varNames[sel]].hidden) sel=sel0; // bumped into end on hidden, revert
                // check if we also need to scroll down
                if(sel_y>(h-8)) {
                    if(scroll<st.varNames.size()-3) ++scroll;
                    while(scroll<st.varNames.size()-3 && st.vars[st.varNames[scroll]].hidden) ++scroll;
                }

                editing=false;
                break;
            case TB_KEY_ENTER:
                if(st.vars[st.varNames[sel]].val.t == T_BOOL) {
                    st.vars[st.varNames[sel]].val.b = !st.vars[st.varNames[sel]].val.b;
                    st.files[st.vars[st.varNames[sel]].fl].s->Execute(st.vars[st.varNames[sel]].fl, &st, true);
                } else if(!editing) {
                    editing=true;
                    stb_textedit_initialize_state(&test, 1);
                    u8tou32(&tecontents, st.vars[st.varNames[sel]].val.s);
                } else if(editing) {
                    editing=false;
                    st.vars[st.varNames[sel]].val.s = u32tou8(&tecontents);
                    st.vars[st.varNames[sel]].val.f = atof(st.vars[st.varNames[sel]].val.s.c_str());
                    st.vars[st.varNames[sel]].val.i = atoi(st.vars[st.varNames[sel]].val.s.c_str());
                    st.vars[st.varNames[sel]].val.b = (bool)st.vars[st.varNames[sel]].val.i;
                    if(st.vars[st.varNames[sel]].val.t != T_STRING)
                        st.vars[st.varNames[sel]].val.s = st.vars[st.varNames[sel]].val.Render();
                    st.files[st.vars[st.varNames[sel]].fl].s->Execute(st.vars[st.varNames[sel]].fl, &st, true);
                }
                break;
            case TB_KEY_ESC:
                if(editing) editing=false;
                else goto abort_interact;
                break;
            case TB_KEY_CTRL_C: 
                for(int i=0;i<st.files.size();++i) {
                    st.files[i].s->Execute(i, &st, true);
                    st.SaveFile(i);
                }
                goto abort_interact;
            case TB_KEY_CTRL_D: goto abort_interact;
            default:
                if(editing) {
                    int mod=0;
                    if(ev.mod & TB_MOD_SHIFT) mod |= STB_TEXTEDIT_K_SHIFT;
                    if(ev.key) stb_textedit_key(&tecontents, &test, ev.key|mod);
                    else {
                        stb_textedit_text(&tecontents, &test, &ev.ch, 1);
                    }
                }
            }
        }
    }
abort_interact:

    tb_shutdown();
}
