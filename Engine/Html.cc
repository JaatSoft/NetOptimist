#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>

#include "Be.h"
#include "platform.h"
#include "Frame.h"
#include "Html.h"
#include "DocElemStack.h"
#include "TagAttr.h"
#include "TagDocElemFactory.h"
#include "StrDocElem.h"
#include "Constraint.h"
#include "DocWalker.h"
#include "Url.h"
#include "UrlQuery.h"
#include "TagStack.h"
#include "Resource.h"
#include "traces.h"
#include "StrPlus.h"
#include "fwt.h"

#include "jsbridge/jsctx.h"

class BufferReader {
	/** This class is a buffer implementation for a Resource object.
	    Beside the method to obtain a char [ method Next() ], it
	    provides :
		- Commit / Rollback to validate or reject tokens
		- InsertText which inserts characters in the parser
		  before any additional char from Resource are processed.
		  WARNING : all chars read must be either commited or rollback
		  before calling this primitive.
		  NOTE : this function has been implemented to support
		  documemt text generated by JavaScript.
	*/
	struct TextRun {
		struct TextRun *m_next;
		int m_size;
		int m_contentLen;
		int m_offset;
	};
	int	m_curp;
	TextRun *m_curt;
	TextRun *m_data;	// Buffer head
	TextRun *m_last;	// Buffer tail
	TextRun *m_free;	// Free blocks
	TextRun *m_insert;
	TextRun *m_insert_last;
	Resource *m_res;
	bool m_end;
	TextRun* NewRun(int len) {
//printf("(NewRun) m-data %p m_last %p m_curt %p\n", m_data, m_last, m_curt);
		TextRun *t = (TextRun*)malloc(len + sizeof(TextRun));
		t->m_contentLen = 0;
		t->m_next = NULL;
		t->m_size = len;
		t->m_offset = 0;
		return t;
	}
	void FreeRun(TextRun *t) {
		free(t);
	}
	void Dump() {
		TextRun* data;
		printf("- insert : \n");
		for (data=m_insert; data; data = data->m_next) {
			printf("TR +%d, nb %d\n", data->m_offset, data->m_contentLen);
		}
		printf("- data : \n");
		for (data=m_data; data; data = data->m_next) {
			printf("TR +%d, nb %d\n", data->m_offset, data->m_contentLen);
		}
		printf("----\n");
	}
	void FreeList(TextRun ** head, TextRun **last) {
		TextRun *t0 = *head;
		while(t0) {
			TextRun *t1 = t0->m_next;
			FreeRun(t0);
			t0 = t1;
		}
		*head = NULL;
		if (last) *last = NULL;
	}
	TextRun *CutHead(TextRun ** head, TextRun **last) {
		TextRun *t = *head;
		if (t) {
			*head = t->m_next;
			if (last && *last==t) *last = t->m_next;
			t->m_next = NULL;
		}
		return t;
	}
	void Insert(TextRun *elem, TextRun ** head, TextRun **last) {
		elem->m_next = *head;
		if (last && *last==NULL) *last = elem;
		*head = elem;
	}
	void Append(TextRun *elem, TextRun ** head, TextRun **last) {
		TextRun * l = NULL;
		if (last)
			l = *last;
		if (!l) {
			if (*head) {
				l = *head;
				while (l->m_next) {
					l = l->m_next;
				}
			}
		}
		if (l) {
			// Chain it
			l->m_next = elem;
		}
		if (!*head) {
			*head = elem;
		}
		if (last) {
			*last = elem;
		}
	}
	void UnchainFirst() {
//printf("(unchain) m-data %p m_last %p m_curt %p\n", m_data, m_last, m_curt);
		TextRun *head;
		head = CutHead(&m_data, &m_last);
		head->m_contentLen = 0;
		head->m_offset = 0;
		Insert(head, &m_free, NULL);
//printf("(unchained) m-data %p m_last %p m_curt %p\n", m_data, m_last, m_curt);
	}
	TextRun *ReadResource() {
//printf("(fill) m-data %p m_last %p m_curt %p\n", m_data, m_last, m_curt);
		TextRun * t = CutHead(&m_free, NULL);
		if (!t) { 
			t = NewRun(1000);
		}
		Append(t, &m_data, &m_last);
		t->m_contentLen = m_res->ReadChar(((char*)t) + sizeof(TextRun), t->m_size);
		m_end = (t->m_contentLen < t->m_size);
//printf("(filled) m_data %p m_last %p m_curt %p\n", m_data, m_last, m_curt);
		return t;
	}
	void Fill2(TextRun *p) {
//Dump();
		if (m_insert) {
			m_curt = NULL; // Or assert !?!?
			m_insert_last->m_next = m_data;
			m_data = m_insert;
			if (!m_last) m_last = m_insert_last;
			m_insert = NULL;
			m_insert_last = NULL;
		}
		if (!m_data || !p || !p->m_next)
			ReadResource();
	}
public:
	void Commit(char * =NULL) {
		if (!m_curt) return;	// nothing to commit
		while (m_curt != m_data) {
			UnchainFirst();
		}
		//m_curt->m_contentLen -= m_curp - m_curt->m_offset;
		m_curt->m_offset = m_curp;
	}
	void Rollback() {
		m_curt = m_data;
		m_curp = m_curt->m_offset;
	}
	void InsertText(const char *p) {
printf("InsertText() : %s\n", p);
		if (m_curt) {
			// Verify that all text has been commited
			if (m_curp != m_curt->m_offset) {
				// panic
				abort();
			}
		}
		m_curp = 0;
		m_curt = NULL;
		TextRun *r = m_insert_last;
		while (*p) {
			if (!r || r->m_contentLen>=r->m_size) {
				r = CutHead(&m_free, NULL);
				if (!r) { 
					r = NewRun(1000);
				}
				Append(r, &m_insert, &m_insert_last);
				r->m_offset = 0;
				r->m_contentLen = 0;
			}
			// Fill this TextRun
			while (*p && r->m_contentLen<r->m_size) {
				// XXX could we use strncpy here ?
				*(((char*)r) + sizeof(TextRun) + r->m_contentLen) = *p;
				r->m_contentLen++;
				p++;
			}
		}
	}
	char Next() {
		if (!m_curt) {
			Fill2(m_curt);
			m_curt = m_data;
			m_curp = m_curt->m_offset;
//printf("(next) m-data %p m_last %p m_curt %p\n", m_data, m_last, m_curt);
		}
		if (m_curp>=m_curt->m_contentLen) {
			if (!m_curt->m_next && !m_end) Fill2(m_curt);

			if (m_curt->m_next) {
				m_curt = m_curt->m_next;
				m_curp = m_curt->m_offset;
			} else {
//printf("(end) m-data %p m_last %p m_curt %p end %d\n", m_data, m_last, m_curt, m_end);
				return -1;
			}
		}
		return ((char*)m_curt)[m_curp++ + sizeof(TextRun)];

	}
	BufferReader(Resource *r) {
		m_res = r;
		m_end = false;
		m_data = m_last = NULL;
		m_free = NULL;
		m_insert = m_insert_last = NULL;
		m_curt = NULL;
	}
	~BufferReader() {
		FreeList(&m_data, &m_last);
		FreeList(&m_insert, &m_insert_last);
		FreeList(&m_free, NULL);
	}
};

DocFormater::~DocFormater() {
	DocElem *iter=last; 
	while(iter!=NULL) {
		DocElem *p=iter->Last(); 
		delete iter;
		iter=p; 
	}
	last = cur = doc = NULL;
}

void DocFormater::Draw(bool onlyIfChanged) {
	DocElem *iter;
	DocWalker walk(doc);
	printf("DocFormater::Draw onlyIfChanged = %d\n", onlyIfChanged);
	while ((iter = walk.Next())) {
		iter->draw(m_frame, onlyIfChanged);
		walk.Feed(iter);
	}
}

bool DocFormater::Select(int x, int y, Action action, UrlQuery *query) {
	/* This function searches for a clickable element and returns
	 * the destination URL. If the location were the user clicked
	 * is not clickable, NULL is returned.
	 * Clickable DocElem are those returning true in IsActive().
	 */

	/* XXX Ok, this could be better in several ways.
	 * XXX - different resulting actions
	 * XXX - we should also support kbd navigation.
	 */

	DocElem *iter;
	if (!doc) return false;
	DocWalker walk(doc);
	DocElem* activeElem = NULL;
	while ((iter = walk.Next())) {
		walk.Feed(iter);
		if (iter->IsActive()) {
			// If the current tag is a link and is above the click point, record it
			if (iter->y<=y) {
				activeElem = iter;
			}
		} else {
			TagDocElem *t = dynamic_cast<TagDocElem*>(iter);
			if (activeElem && t && t->isClosing() && t->Parent() == activeElem) {
				// This closes the previous tagert candidate, clear it
				activeElem = NULL;
			}
		}
		if (activeElem && iter->x<=x && iter->y<=y && iter->x+iter->w>x && iter->y+iter->h>y) {
			if (activeElem->Action(action, query)) {
				const char *url_text = query->Url();
				if (url_text) {
					char msg[1024];
					snprintf(msg, sizeof(msg)-1, "link to %s", query->Url());
					m_frame->Message(msg);
				} else {
					fprintf(stderr, "DocElem %d is active but does not provide any url\n", activeElem->id);
				}
				return url_text != NULL;
			}
		}
		if (activeElem==iter && iter->Included()==NULL)
			activeElem=NULL;
	}
	return false;
}

void DocFormater::format() {

	DocElem *iter;

#ifdef MALLOC_INFO // We don't have mallinfo on beos ?
	struct mallinfo alloc_info_start;
	struct mallinfo alloc_info_end;
	alloc_info_start = mallinfo();
#endif
	if (doc) {
		DocWalker walk(doc);
		while ((iter = walk.Next())) {
			iter->initPlacement();
			walk.Feed(iter);
		}
	
		if (!m_relationAlreadySet) {
			fprintf(stderr, "Error RelationSet not called\n");
		}
	
		for (iter=last; iter!=NULL; iter=iter->Last()) {
			// XXX Testing for iter->constraint is the only way i have
			// XXX found to know if this tag is hidden
			if (iter->constraint)
				iter->dynamicGeometry();
		}

		doc->constraint->Init(0,0, m_frame->DocWidth());
		iter = doc;
		DocWalker walk2(doc);
		while ((iter = walk2.Next())) {
			iter->place();
			int right,bottom,top;
			top=iter->y;
			bottom = iter->constraint->NextLineY() - 1;
			if (iter->includedConstraint)
				bottom = max(bottom, iter->includedConstraint->NextLineY() - 1);
			right  = iter->x+iter->w-1;
			DocElem *parent = NULL;
			if (iter->constraint->creator!=iter)
				parent = iter->constraint->creator;
			while (parent) {
				// XXX we should try to simplify this loop and exit as soon as parent->update() return false
				// but is this ok ?
				bool needed = parent->update(right,top,bottom);
				/* XXX No ! not good :
					- TD_DocElem::place calls update()
					- the needed flag should be put on DocElem::place()
				*/
				needed = true; // XXX Force the whole hierachy to be updated
				if (needed) {
					right=parent->x+parent->w -1;
					top  =parent->y;
					bottom=parent->y+parent->h -1;
					if (parent->constraint->creator!=parent)
						parent = parent->constraint->creator;
					else 
						parent = NULL;
				} else {
					parent = NULL;
				}
			}
			walk2.Feed(iter);
		}
		if (doc->constraint)
			m_frame->SetFrame(doc->constraint->Width(), doc->constraint->NextLineY());
	}
#ifdef MALLOC_INFO // We don't have mallinfo on beos ?
	alloc_info_end = mallinfo();

	fprintf(stderr, "FORMAT : new ordinary blocks : %d\n", alloc_info_end.ordblks - alloc_info_start.ordblks);
	fprintf(stderr, "FORMAT : new small blocks : %d\n", alloc_info_end.smblks - alloc_info_start.smblks);
#endif
}


enum{ /* THESE MUST BE BIT-WISE EXCLUSIVE */
	HTML_PRE = 0x1,
	HTML_NOBR = 0x2,
	HTML_COMMENT = 0x4
};

# define HTML_RAW (HTML_COMMENT)

/* substitues &seq;-style charactes in string */
void DocFormater::html_ctrlchar_alter(char *wholestr, char *& ptr) {
	int pos = ptr - wholestr;
	for(int i=pos; pos-i < 15 && i>=0; i--) {
		if (wholestr[i]=='&') {
			if (!strcmp(wholestr+i+1,"#183;")) {
				/* We are repacing numeric values by symbolic name */
				strcpy(wholestr+i+1,"middot;");
				// continue
			}
#ifdef __BEOS__
			if (!strcmp(wholestr+i+1,"eacute;")) {
				strcpy(wholestr+i,"é");
				ptr = (wholestr+i) + 2;
				return;
			}
			if (!strcmp(wholestr+i+1,"egrave;")) {
				strcpy(wholestr+i,"è");
				ptr = (wholestr+i) + 2;
//				int32 state = 0;
//				int32 len = 2;
//				int32 olen = 2;
//				convert_to_utf8(B_ISO1_CONVERSION, "è", &len, wholestr+i, &olen, &state);
//				ptr = (wholestr+i) + 2;
				return;
			}
			if (!strcmp(wholestr+i+1,"copy;")) {
				strcpy(wholestr+i,"©");
				ptr = (wholestr+i) + strlen("©");
				return;
			}
#endif
			if (!strcmp(wholestr+i+1,"middot;")) {
				strcpy(wholestr+i,".");
				ptr = (wholestr+i) + 1;
				return;
			}
			if (!strcmp(wholestr+i+1,"szlig;")) {
				strcpy(wholestr+i,"ss");
				ptr = (wholestr+i) + 2;
				return;
			}
			if (!strcmp(wholestr+i+1,"copy;")) {
				strcpy(wholestr+i,"(c)");
				ptr = (wholestr+i) + 3;
				return;
			}
			if (!strcmp(wholestr+i+1,"reg;")) {
				strcpy(wholestr+i,"(r)");
				ptr = (wholestr+i) + 3;
				return;
			}
			// Replace '?circ;' by '?'
			if (!strcmp(wholestr+i+2,"circ;")) {
				wholestr[i] = wholestr[i+1];
				wholestr[i+1] = '\0';
				ptr = (wholestr+i) + 1;
				return;
			}
			// Replace '?acute;' by '?'
			if (!strcmp(wholestr+i+2,"acute;")) {
				wholestr[i] = wholestr[i+1];
				wholestr[i+1] = '\0';
				ptr = (wholestr+i) + 1;
				return;
			}
			// Replace '?grave;' by '?'
			if (!strcmp(wholestr+i+2,"grave;")) {
				wholestr[i] = wholestr[i+1];
				wholestr[i+1] = '\0';
				ptr = (wholestr+i) + 1;
				return;
			}
			// Replace '?uml;' by '?'
			if (!strcmp(wholestr+i+2,"uml;")) {
				wholestr[i] = wholestr[i+1];
				wholestr[i+1] = '\0';
				ptr = (wholestr+i) + 1;
				return;
			}
			if (!strcmp(wholestr+i+1,"iexcl;")) {
				strcpy(wholestr+i,"!");
				ptr = (wholestr+i) + 1;
				return;
			}
			if (!strcmp(wholestr+i+1,"iquest;")) {
				strcpy(wholestr+i,"?");
				ptr = (wholestr+i) + 1;
				return;
			}
			if (!strcmp(wholestr+i+1,"ntilde;")) {
				strcpy(wholestr+i,"n");
				ptr = (wholestr+i) + 1;
				return;
			}
			if (!strcmp(wholestr+i+1,"ccedil;")) {
				strcpy(wholestr+i,"c");
				ptr = (wholestr+i) + 1;
				return;
			}
			if (!strcmp(wholestr+i+1,"raquo;")) {
				strcpy(wholestr+i,">");
				ptr = (wholestr+i) + 1;
				return;
			}
			if (!strcmp(wholestr+i+1,"mdash;")) {
				strcpy(wholestr+i,"-");
				ptr = (wholestr+i) + 1;
				return;
			}
			if (!strcmp(wholestr+i+1,"amp;")) {
				strcpy(wholestr+i,"&");
				ptr = (wholestr+i) + 1;
				return;
			}
			if (!strcmp(wholestr+i+1,"nbsp;")) {
				strcpy(wholestr+i," ");
				ptr = (wholestr+i) + 1;
				return;
			}
			if (!strcmp(wholestr+i+1,"lt;")) {
				strcpy(wholestr+i,"<");
				ptr = (wholestr+i) + 1;
				return;
			}
			if (!strcmp(wholestr+i+1,"quot;")) {
				strcpy(wholestr+i,"\"");
				ptr = (wholestr+i) + 1;
				return;
			}
			if (!strcmp(wholestr+i+1,"apos;")) {
				strcpy(wholestr+i,"'");
				ptr = (wholestr+i) + 1;
				return;
			}
			if (!strcmp(wholestr+i+1,"gt;")) {
				strcpy(wholestr+i,">");
				ptr = (wholestr+i) + 1;
				return;
			}
			if (!strcmp(wholestr+i+1,"#146;")) {
				strcpy(wholestr+i,"'");
				ptr = (wholestr+i) + 1;
				return;
			}
			if (wholestr[i+1]=='#') {
				strcpy(wholestr+i,"*");
				ptr = (wholestr+i) + 1;
				return;
			}
		}
	}
}

/* Process one tag and its attributes
	note : the leading '<' is already eaten
*/
TagDocElem* DocFormater::html_parse_tag() {
	char buf[10000];
	char *ptr;
	int ch;
	buf[0] = '\0';
	ptr = buf;
	Tag *thisTag = NULL;
	TagAttr *attrList = NULL;
	while((ch=m_bufferReader->Next())!=-1) 
	{
		if (((unsigned)(ptr-buf))>=sizeof(buf)-1000) {
			/* Some tag attributes can be very long (comments, javascript, keywords, ...) */
			// XXX We must find a way to handle them, probably malloc/realloc.
			ptr--;
		}
		switch(ch) {
		case '\"':
			*(ptr++)=ch;
			if (m_parseMode & HTML_RAW) {
				// in HTML comments, we ignore "
				break;
			}
			if (ptr-buf>=2 && *(ptr-2)=='=') {
				/* HACK I saw this on /. (note that the number of " doesn't match) :
				 * HACK <INPUT TYPE="HIDDEN" NAME="op" VALUE="userlogin" %]">
				 * HACK This is why there is such a test above, let's see if
				 * HACK something else breaks
				 */

				/* This seems to be a string of type `href="' */
				while((ch=m_bufferReader->Next())!=-1 && ch!='\"') {
					*(ptr++)=ch;
				}
				m_bufferReader->Commit("attr='...'");
				*(ptr++)='\"';
				*(ptr++)='\0';
				if (thisTag != NULL) {
					/* we are creating the tag attribute here as some pages has `border="0"alt="xxx"' 
					   without any space between tag attributes */
					trace(DEBUG_TAGPARSE) printf("TAG: found tag attr(string) %s\n", buf);
					attrList = new TagAttr(buf,attrList);
					ptr = buf;
				}
			}
			break;
		case '-':
			/* XXX TODO : test that "var--;" code does not end HTML_COMMENT mode */
			if ((m_parseMode & HTML_COMMENT) && ptr!=buf && *(ptr-1)=='-') {
				*(ptr-1)='\0';
				m_parseMode &= ~HTML_COMMENT;
			} else {
				*(ptr++)=ch;
			}				
			if (thisTag == NULL && ptr-buf==3 && !(m_parseMode & HTML_RAW)) {
				// We have read 3 chars : this could be an html comment : <!--... -->
				*ptr = '\0';
				if (!strcmp(buf,"!--")) {
					trace(DEBUG_TAGPARSE) printf("TAG: new html comment\n");
					m_parseMode |= HTML_COMMENT;
					ptr = buf;
					trace(DEBUG_TAGPARSE) printf("TAG: found attr %s\n", buf);
					thisTag = new Tag(buf);
				}
			}
			break;
		case '>':
		case '\n':
		case '\r':
		case '\t':
		case ' ':
			if (m_parseMode & HTML_RAW) {
				*(ptr++)=ch;
				break;
			}
			*ptr = '\0'; // space and \n are seen as separators
			if (!strnull(buf)) {
				if (thisTag == NULL) {
					trace(DEBUG_TAGPARSE) printf("TAG: found tag %s\n", buf);
					thisTag = new Tag(buf);
					m_bufferReader->Commit();
				} else {
					if (strcmp(buf, "/")) {
						trace(DEBUG_TAGPARSE) printf("TAG: found tag attr %s\n", buf);
						attrList = new TagAttr(buf,attrList);
						m_bufferReader->Commit();
					}
				}
			}
			ptr = buf;
			if (ch=='>') {
				if (thisTag) {
					if (thisTag && !strcasecmp(thisTag->toString(), "script")) {
						char ch;
						bool end = false;
						m_bufferReader->Commit();
						ptr = buf;
						while(!end && (ch=m_bufferReader->Next())!=-1) {
							*(ptr++)=ch;
							if (ch=='>') {
								*ptr = '\0';
								/* This could be a closing script tag (</script>) */
								char *c = ptr-1;
								while (c>buf && *c!='<') c--;
								if (*c=='<') {
									const char *p = "</script>";
									while (c<ptr && *p) {
										if (tolower(*c)==*p) {
											c++; p++;
										} else {
											// XXX spaces cannot be just anywhere eg < /s c r i p t >
											if (*c==' ' || *c=='\t' || *c=='\n') {
												c++;
											} else {
												break;
											}
										}
									}
									while (c<ptr && *c==' ' || *c=='\t' || *c=='\n') { c++; }
									if (*p=='\0' && c==ptr) {
										char *code = ptr;
										m_bufferReader->Commit();

										// He we found it !!! the </script> tag...
										// remove it from the code :
										while (code>buf && *code!='<') code--;
										if (*code=='<') *code = '\0';
										// remove --> a the end of the script
										code--;
										while (code > buf && 
											(*code =='\t' || *code==' ' || *code=='\n')) 
											code--;
										if (code-2>=buf && strprefix(code-2,"-->"))
											*(code-2) = '\0';
										// now remove <!-- in the front :
										code = buf;
										printf("code font = %d\n", code[0]);
										while (*code == '\r' || *code == '\n' || *code == ' ' || *code =='\t') code++;
										if (strprefix(code, "<!--")) code+=4;
										printf("found '</script> code = %s'\n", code);
										m_jsctx->Execute(code);

										trace(DEBUG_TAGPARSE) printf("TAG: end of script tag\n");
										ptr = buf;
										end = true;
									}
								}
							}
						}
					}
					// XXX the script source is discarded
					m_bufferReader->Commit();
					trace(DEBUG_TAGPARSE) printf("TAG: end of tag\n");
					return TagDocElemFactory::New(thisTag, attrList);
				} else {
					m_bufferReader->Rollback();
					return NULL; // XXX something get wrong, maybe a <> ?
				}
			}
			break;
		default:
			*(ptr++)=ch;
		}
	}
	*(ptr++)='\0';
	fprintf(stderr, "error in html_parse_tag,tag = %s...\n", buf);
	return NULL;
}

void DocFormater::CloseDocElem(const Tag* nextTag) {
	/* Closes an opened tag. Move the current doc elem (DocFormater::cur) to the closed tag
	 * eg. next is </TD>, this function searches the matching <TD>, mark it closed,
	 * cur is moved to <TD>
	 */

	for (DocElem *iter = cur->m_prev; iter != NULL; ) {
		// If we were able to close a tag, current DocElem
		// must point to the closed tag
		if ( iter->close(nextTag,cur) ) {
			// We have successfully closed a tag
			// The current DocElem is now the closed tag
			cur = iter;
			iter = NULL;
		} else {
			if (iter->m_up)
				iter = iter->m_up;
			else
				iter = iter->m_prev;
		}
	}
}

void DocFormater::ForceCloseHead() {
	/* The purpose of this method is to close the tag at the stack head.
	 * There a 2 kinds of closeMode for tags on the stack : CAN_CLOSE and ALWAYS_CLOSE.
	 * We only close those which need a closing tag. The others are just pop'ed off the
	 * stack.
	 */
	if (m_openedTags->head()->info && m_openedTags->head()->info->closeMode == ALWAYS_CLOSE) {
		TagDocElem *next = new TagDocElem(new Tag(m_openedTags->head()->toString(),true));
		trace(DEBUG_CORRECT) {
			fprintf(stdout, "ForceCloseHead fakes <%s> tag\n",
				next->toString());
		}
		AddTagDocElem(next);
		CloseDocElem(next->t);
	}
	m_openedTags->pop(); // Remove this tag from the stack
}

void DocFormater::AddDocElem(DocElem *elem) {
	if (cur == NULL) {
		doc = cur = elem;
	} else {
		cur->add(elem, last);
		cur = elem;
	}
	last = cur;
}

void DocFormater::AddTagDocElem(TagDocElem *elem) {
	const char *name = elem->toString();
	if (!strcasecmp(name,"PRE")) {
		m_parseMode |= HTML_PRE;
	} else if (!strcasecmp(name,"/PRE")) {
		m_parseMode &= ~HTML_PRE;
	} else if (!strcasecmp(name,"NOBR")) {
		m_parseMode |= HTML_NOBR;
	} else if (!strcasecmp(name,"/NOBR")) {
		m_parseMode &= ~HTML_NOBR;
	}
	AddDocElem(elem);
}

enum TagClass {
	TG_SIMPLE = 0,
	TG_CELL = 1,
	TG_ROW = 2,
	TG_ROWGROUP = 3,
	TG_TABLE = 4
};

TagClass getTagClass(const char *tagStr) {
	if (!strcmp(tagStr,"TABLE")) return TG_TABLE;
	if (!strcmp(tagStr,"TBODY")) return TG_ROWGROUP;
	if (!strcmp(tagStr,"THEAD")) return TG_ROWGROUP;
	if (!strcmp(tagStr,"TFOOT")) return TG_ROWGROUP;
	if (!strcmp(tagStr,"TR")) return TG_ROW;
	if (!strcmp(tagStr,"TH")) return TG_CELL;
	if (!strcmp(tagStr,"TD")) return TG_CELL;
	return TG_SIMPLE;
}

/* Main html parser */
void DocFormater::parse_html(Resource *resource) {
	int ch;
	char *ptr;
	char *curStr;
	char *prevStr;
	char normal_text[10000] ;

#ifdef MALLOC_INFO // We don't have mallinfo on beos ?
	struct mallinfo alloc_info_start;
	struct mallinfo alloc_info_end;
	alloc_info_start = mallinfo();
#endif
	m_parseMode = 0;
	m_openedTags = new TagStack;

	m_jsctx = new JsCtx;
	m_jsctx->Init(this);

	m_relationAlreadySet = false;

	bool found = resource && resource->Open();
	
	if (!found) {
		strcpy(normal_text, "Url not found ! Check the host name in the URL.");
		DocElem *newStrElem  = new StrDocElem(normal_text);
		AddDocElem(newStrElem);
	}

	m_bufferReader = new BufferReader(resource);

	curStr = normal_text;
	curStr[0]='\0';
	ptr = curStr;
	prevStr = NULL;
	if (found) while((ch=m_bufferReader->Next())!=-1) 
	{
		switch(ch) {
		case ' ':
			if (m_parseMode & HTML_NOBR) {
				// spaces are treated as normal chars
				break;
			}
			// FALL THROUGH
		case '\t':
			if (m_parseMode & HTML_PRE) {
				// spaces are treated as normal chars
				break;
			}
			// FALL THROUGH
		case '<':
		case '\n':
		case '\r':
			if (curStr[0]!='\0') {
				*ptr = '\0';
				trace(DEBUG_PARSER)
					printf("<STR(%ld) '%s'>%c",strlen(curStr),curStr,ch!=' '?'\n':' ');
				DocElem *newStrElem  = new StrDocElem(curStr);
				AddDocElem(newStrElem);
				curStr[0]='\0';
				ptr = curStr;
				m_bufferReader->Commit();
			}
		}
		switch(ch) {
		case ' ':
			if (m_parseMode & HTML_NOBR) {
				*(ptr++)=ch;
				break;
			}
		case '\t':
			if (m_parseMode & HTML_PRE) {
				*(ptr++)=ch;
			}
			break;
		case '\n':
		case '\r':
			if (m_parseMode & HTML_PRE) {
				TagDocElem *newTagElem  = new TagDocElem(new Tag("BR"));
				AddTagDocElem(newTagElem);
			}
			break;
		case '<':
			{
				m_bufferReader->Commit();
				TagDocElem *newTagElem  = html_parse_tag();
				
				if (!newTagElem) {
					/* This is the worst that can happen : the parser get wrong.
					 * The best thing we can do here is to tell it to the user
					 */
 					DocElem* next = new StrDocElem("[[ NetOptimist error : some part of the document may be lost ]]");
					AddDocElem(next);
					break;
				}

				trace(DEBUG_LEXER) {
					printf("TOKEN_READ : %s\n",newTagElem->toString());
				}
				trace(DEBUG_PARSER) {
					int depth=m_openedTags->height();
					for (int i=0; i<depth; i++)
						printf(" ");
					printf("<%s(%d)",newTagElem->toString(), newTagElem->id);
					for (TagAttr *attr = newTagElem->list; attr!=NULL; attr = attr->Next()) {
						if (attr->AttrName() && attr->AttrValue())
							printf(" %s=%s",attr->AttrName(),attr->AttrValue());
					}
					printf(">\n");
				}
				trace(DEBUG_TAGINPAGE) {
					DocElem* next = new StrDocElem(newTagElem->toString());
					AddDocElem(next);
				}
				bool closeSuccess = false;

				if (!newTagElem->isClosing()) {
					TagClass curTagClass = getTagClass(newTagElem->toString());

					// XXX We need something more generic
					if (curTagClass != TG_SIMPLE && curTagClass != TG_TABLE) {
						// ---- This section verifies that <TD> is closed before adding <TR>
						int k;
						for (k=m_openedTags->height()-1; k>=0; k--) {
							TagClass iterTagClass = getTagClass(m_openedTags->ElementAt(k)->toString());
							if (curTagClass < iterTagClass) {
								break;
							}
						}
						if (k>=0 && m_openedTags->height()-1>k) {
							/* We have found a "stronger" Tag, we clean the top of the stack
							 * until this stronger tag (excluded) 
							 */
							trace(DEBUG_CORRECT) {
								fprintf(stdout, "READ <%s>(%d) --> OP_CLOSE_FORCE(rule TR over TD)\n",
									newTagElem->toString(), curTagClass);
								m_openedTags->dump();
							}
							while(m_openedTags->height()-1>k) {
								trace(DEBUG_CORRECT) {
									fprintf(stdout, " ... closing %s\n", m_openedTags->head()->toString());
								}
								ForceCloseHead();
							}
						}
					}
					// ---- Verify that a similar tag (that cannot be reopened) is not already opened
					if (newTagElem->t->info && newTagElem->t->info->canReopen==false) {
						// We must close previous tag of same kind
						if (m_openedTags->height()>0 && !strcmp(m_openedTags->head()->toString(),newTagElem->toString())) {
							trace(DEBUG_CORRECT) {
								printf("READ <%s> stackhead <%s> --> OP_CLOSE_DUPLICATE\n",newTagElem->toString(),m_openedTags->head()->toString());
								m_openedTags->dump();
							}
							ForceCloseHead();
						}
					}

					// ---- This section verify that <TR> is open before adding <TD>
					// #### We need something more generic
					if (!strcmp(newTagElem->toString(),"TD")) {
						bool tr_found = false;
						for (int k=m_openedTags->height()-1; k>=0; k--) {
							if (!strcmp(m_openedTags->ElementAt(k)->toString(),"TR")) {
								tr_found = true;
								break;		// Exits for-loop
							}
							if (!strcmp(m_openedTags->ElementAt(k)->toString(),"TABLE")) {
								break;		// Exits for-loop
							}
						}
						if (!tr_found) {
							fprintf(stdout, "TR has been added before TD");
							if (m_openedTags->height()>0)
								 fprintf(stdout, "-- stack head was %s\n", m_openedTags->head()->toString());
							else
								 fprintf(stdout, "-- stack was empty\n");
								 
							TagDocElem *tr_DocElem =
								TagDocElemFactory::New(new Tag("TR"), NULL);
							m_openedTags->push(tr_DocElem->toString());
							AddTagDocElem(tr_DocElem);
						}
					}

					// ---- This section verify that <DT> is closed before adding another <DT>
					// "can reopen" in not enough because some <DD> can be on the stack
					// #### We need something more generic
					if (!strcmp(newTagElem->toString(),"DT")) {
						bool dt_found = false;
						for (int k=m_openedTags->height()-1; k>=0; k--) {
							if (!strcmp(m_openedTags->ElementAt(k)->toString(),"DT")) {
								dt_found = true;
								break;		// Exits for-loop
							}
							if (!strcmp(m_openedTags->ElementAt(k)->toString(),"DL")) {
								break;		// Exits for-loop
							}
						}
						while (dt_found && m_openedTags->height()>0) {
							if (!strcmp(m_openedTags->head()->toString(),"DT"))
								dt_found = false;
							ForceCloseHead();
						}
					}
					// ----

					if (newTagElem->t->info && newTagElem->t->info->closeMode!=NEVER_CLOSE) {
						m_openedTags->push(newTagElem->toString());
						trace(DEBUG_PARSER) {
							printf("OP_ADDED");
							m_openedTags->dump();
						}
					}

					/* end openning tag */
				} else {
					/* This is a closing tag */
					closeSuccess = true;
					trace(DEBUG_PARSER) {
						printf("OP_CLOSE cur=%d %s\n", cur->id, newTagElem->toString());
						m_openedTags->dump();
					}
					if (newTagElem->t->info != NULL) {
						int depth = 0; /* This is used for debug only :
									we are recording where the opening tag were on the stack */
						// Clean the openedTags stack
						const char *openingName = newTagElem->t->info ? TagInfo::Name(newTagElem->t->info) : NULL;
						TagClass curTagClass = getTagClass(openingName);
						if (openingName && m_openedTags->contains(openingName)>=0) {
							while (m_openedTags->height()>0 && !newTagElem->t->closes(m_openedTags->head())) {
#if 1
								TagClass iterTagClass = getTagClass(m_openedTags->head()->toString());
								if (curTagClass < iterTagClass) {
									/* This test ensure that /TR or /TD will not close a containing TABLE
									 * that /TD will not close a containing TR
									 * that /CENTER will not close a containing TR
									 * ...
									 */
									closeSuccess = false;
									fprintf(stdout, "ERROR : could not find opening tag when adding %s\n", newTagElem->toString());
									m_openedTags->dump();
									break;
								}
#else
								if ((!strcmp(newTagElem->toString(),"/TD") || (!strcmp(newTagElem->toString(),"/TR")))
									&& !strcmp(m_openedTags->head()->toString(),"TABLE")) {
									/* This ensure that /TR or /TD will not close a containing TABLE */
									closeSuccess = false;
									break;
								}
								if (!strcmp(newTagElem->toString(),"/CENTER") && !strcmp(m_openedTags->head()->toString(),"TD")) {
									/* This ensure that /CENTER will not close a containing TR */
									closeSuccess = false;
									break;
								}
								if (!strcmp(newTagElem->toString(),"/TD") && !strcmp(m_openedTags->head()->toString(),"TR")) {
									/* This ensure that /TD will not close a containing TR */
									closeSuccess = false;
									break;
								}
#endif
								trace(DEBUG_CORRECT) {
									printf("READ <%s> stackhead <%s> --> OP_CLOSE_IMPLICIT\n",newTagElem->toString(),m_openedTags->head()->toString());
									depth++;
								}
								ForceCloseHead();
							}
							if (m_openedTags->height()==0) {
								closeSuccess = false;
							}
						} else {
							closeSuccess = false;
						}
						trace(DEBUG_PARSER) {
							if (!closeSuccess) {
								printf("OP_CLOSED : Tag %s not on stack\n", newTagElem->toString());
								m_openedTags->dump();
							} else {
								if (depth>0) {
									printf("OP_CLOSED\n");
									m_openedTags->dump();
								}
							}
						}
					}
				}
				if (!newTagElem->isClosing() || closeSuccess) {
					// Actually add the tag
					AddTagDocElem(newTagElem);
				}

				if (closeSuccess) {
					CloseDocElem(newTagElem->t);
					if (m_openedTags->height()>0 && newTagElem->t->closes(m_openedTags->head())) {
						m_openedTags->pop();
					} else {
						fprintf(stdout, "ERROR : could not pop() opened tags when adding %s\n", newTagElem->toString());
						m_openedTags->dump();
					}
				}
			}
			break;
		case ';':
			*(ptr++)=ch;
			*(ptr)='\0';
			html_ctrlchar_alter(curStr,ptr);
			break;
		default:
			*(ptr++)=ch;
			break;
		}
	}
	m_openedTags->dump();
	if (found) resource->Close();

	// -------- This is POST-PARSING --------
	DocWalker walk(doc);
	DocElem *iter;

	while ((iter = walk.Next())) {
		if (!m_relationAlreadySet) {
			iter->RelationSet(m_frame);
		}
		walk.Feed(iter);
	}
	if (!m_relationAlreadySet) {
		/* Do it once but after relation are set */
#ifndef __BEOS__
		ExportToFWT(doc);
#endif
	}
	m_relationAlreadySet = true;

	delete m_openedTags;

#ifdef MALLOC_INFO // We don't have mallinfo on beos ?
	alloc_info_end = mallinfo();

	fprintf(stderr, "new ordinary blocks : %d\n", alloc_info_end.ordblks - alloc_info_start.ordblks);
	fprintf(stderr, "new small blocks : %d\n", alloc_info_end.smblks - alloc_info_start.smblks);
#endif
}

void DocFormater::InsertText(const char *text, int len) {
	char *code = new char[len+1];
	memcpy(code, text, len);
	code[len] = '\0';
	m_bufferReader->InsertText(code);
	delete[] code;
}