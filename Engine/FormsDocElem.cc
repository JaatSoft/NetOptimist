#include "FormsDocElem.h"
#include "StrDocElem.h"
#include "DocWalker.h"
#include "Url.h"
#include "Frame.h"
#include "Style.h"
#include "StrPlus.h"

void FORM_DocElem::Register(INPUT_DocElem *field) {
	if (ISTRACE(DEBUG_FORMS)) {
		printf("FORM %s : found a new field : %s\n", !strnull(m_name)?m_name:m_actionUrl,
							field->Name());
	}
	m_fields[m_nbFields] = field;
	m_nbFields++;
}

void FORM_DocElem::RelationSet(HTMLFrame *view) {
	const char *method=NULL;
	for (TagAttr *iter = list; iter!=NULL; iter=iter->Next()) {
		iter->ReadStrp("action", & m_actionUrl);
		iter->ReadStrp("method", & method);
		iter->ReadStrp("name", & m_name);
	}

	if (!strnull(method)) {
		m_method = METHOD_BUGGY;
		if (!strcasecmp(method, "GET")) m_method=METHOD_GET;
		else if (!strcasecmp(method, "POST")) m_method=METHOD_POST;
		if (m_method == METHOD_BUGGY) {
			fprintf(stderr, "Warning no submit method for form %d\n", id);
		}
	}

	TagDocElem::RelationSet(view);
}

bool FORM_DocElem::Action(::Action /*action*/, UrlQuery * query) {
	int activeFieldsCount;
	activeFieldsCount = 0;
	printf("Action on form url=%s method=%d\n", m_actionUrl, m_method);
	for (int k = 0; k<m_nbFields; k++) {
		if (m_fields[k]->Activated()) activeFieldsCount ++;
	}
	query->SetUrl(m_actionUrl);
	query->m_name = NULL;
	query->m_nbFields = activeFieldsCount;
	query->m_method = m_method;
	char fieldString[1024];
	query->m_fields = new char*[activeFieldsCount];
	for (int i = 0, j=0; j<activeFieldsCount && i<m_nbFields; i++) {
		if (m_fields[i]->Activated()) {
			m_fields[i]->PrintState(fieldString);
			query->m_fields[j] = StrDup(fieldString);
			j++;
		}
	}
	return true;
}

bool INPUT_DocElem::Activated() const {
	return m_activated;
}

bool INPUT_DocElem::IsActive() {
	return m_type == T_SUBMIT
	|| m_type == T_IMAGE
	|| m_type == T_RADIO
	|| m_type == T_CHECKBOX
	|| m_type == T_IMAGE
	|| m_type == T_RESET;
}

bool INPUT_DocElem::Action(::Action action, UrlQuery * href) {
	href->m_name = Text();
	if (action != MOUSE_CLICK) { return false; }

	switch(m_type)
	{
		case T_SUBMIT:
		case T_IMAGE:
			{
			m_activated = true;
			printf("Action on submit/image -> calling action on form\n");
			bool actionSuccess = m_container->Action(action, href);
			m_activated = false;
			return actionSuccess;
			break;
			}
		case T_RADIO:
		case T_CHECKBOX:
			m_activated = ! m_activated;
				// XXX TODO : redraw
				// XXX TODO : handle radio mode.
			break;
		case T_RESET:
			href->SetUrl("Reset");
			break;
		default:
			// Ooops ?
			fprintf(stderr, "INPUT_DocElem::Action : unknown action for type %d\n", m_type);
			break;
	}
	return (href->Url() != NULL);
}

void INPUT_DocElem::PrintState(char*str) {
	const char *type;
	switch(m_type)
	{
		case T_TEXT:
			type = "TEXT"; break;
		case T_RADIO:
			type = "RADIO";
			break;
		case T_CHECKBOX:
			type = "CHECKBOX";
			break;
		case T_HIDDEN:
			type = "HIDDEN";
			break;
		case T_IMAGE:
			type = "IMAGE"; break;
		case T_SUBMIT:
			type = "SUBMIT"; break;
		case T_RESET:
			type = "RESET"; break;
		default:
			type = "Ooops " __FILE__;
	}
	sprintf(str, "%s=%s", m_name, m_value);

	const char *state ="none";
	state = m_activated ? "set" : "not set";
	printf("querying %s state=%s <<%s>>\n", type, state, str);
}

const char *INPUT_DocElem::Text() const {
	if (!strnull(m_value)) {
		return m_value;
	}
	switch(m_type)
	{
		case T_SUBMIT:
			return "Submit";
		case T_RESET:
			return "Reset";
		default:
			return "Ooops " __FILE__;
	}
}

void INPUT_DocElem::geometry(HTMLFrame *view) { 
	int attr_width = -1;
	int attr_height = 10;
	int attr_size = -1;
	char attr_type[10];
	Style *s;

	TagDocElem::geometry(view);
	attr_type[0] = '\0';

	for (TagAttr *iter = list; iter!=NULL; iter=iter->Next()) {
		iter->ReadStrl("type",attr_type, sizeof attr_type);
		iter->ReadStrl("name",m_name, sizeof m_name);
		iter->ReadStrl("value",m_value, sizeof m_value);
		iter->ReadInt("width",&attr_width);
		iter->ReadInt("height",&attr_height);
		iter->ReadInt("size",&attr_size);
	}
	if (!strcasecmp(attr_type, "hidden")) {
		m_type = T_HIDDEN;
	} else if (!strcasecmp(attr_type, "submit")) {
		m_type = T_SUBMIT;
	} else if (!strcasecmp(attr_type, "reset")) {
		m_type = T_RESET;
	} else if (!strcasecmp(attr_type, "checkbox")) {
		m_type = T_CHECKBOX;
	} else if (!strcasecmp(attr_type, "radio")) {
		m_type = T_RADIO;
	} else if (!strcasecmp(attr_type, "image")) {
		m_type = T_IMAGE;
	} else if (!strcasecmp(attr_type, "text") || !strcasecmp(attr_type, "name") || !strcasecmp(attr_type, "password")) {
		m_type = T_TEXT;
	} else if (strnull(attr_type)) {
		// When no type is suplied, TEXT is the default
		m_type = T_TEXT;
	} else {
		m_type = T_BOGGUS;
	}
	switch(m_type)
	{
		case T_SUBMIT:
		case T_RESET:
			{
				int strw, strh;
				view->StringDim(Text(), m_style,&strw,&strh);
				h=max(attr_height, strh+10);
				w=max(attr_width, strw+20);
				fixedW=w;
				s = m_style->clone(id);
				s->SetBGColor(kGray);
				s->SetColor(kBlack);
				m_style = s;
			}
			break;
		case T_RADIO:
		case T_CHECKBOX:
			h=12;
			w=12;
			fixedW=w;
			break;
		case T_TEXT:
		case T_PASSWORD:
			m_activated = true;
			h=20;
			w=0;
			s = m_style->clone(id);
			s->SetBGColor(kWhite);
			s->SetColor(kBlack);
			m_style = s;
			if (attr_width>0 && attr_size<0) {
				// width is in pixels. However, if size (in char) is supplied,
				// we ignore this attribute
				w = attr_width;
			}
			if (attr_size>0) {
				// using the width of a const string to have an
				// approximation of the width
				int dummy;
				view->StringDim("0123456789", s, &w, &dummy);
				w = w/10;
				printf("input size = %d, w(_)=%d\n", attr_size, w);
				w = w * attr_size + 20;
			}
			if (w<=0) w=50;
			fixedW=w;
			break;
		case T_IMAGE:
			h=attr_height;
			w=max(attr_width, 30);
			fixedW=w;
			break;
		case T_HIDDEN:
			m_activated = true;
			// The tag is hidden : sizes = 0
			break;
		case T_BOGGUS:
		default:
			h=12;
			w=20;
			fixedW=w;
			break;
	}
}

void INPUT_DocElem::RelationSet(HTMLFrame *view) {
	TagDocElem::RelationSet(view);
	DocElem *p = Predecessor();
	while(p != NULL) {
		m_container = dynamic_cast<FORM_DocElem*>(p);
		if (m_container) {
			break;
		}
		p = p->Predecessor();
	}
	if (m_container) {
		m_container->Register(this);
	} else {
		fprintf(stderr, "ERROR : did not Found container FORM\n");
	}

}

void INPUT_DocElem::draw(HTMLFrame *view, bool /*onlyIfChanged*/) { 
	switch(m_type)
	{
		case T_SUBMIT:
		case T_RESET:
			view->DrawBorder3D(x+1,y+1,w-2,h-2,m_style,3, HTMLFrame::LOOK_UP);
			view->DrawString(x+9,y+h-6,w-20,Text(),m_style);
			break;
		case T_IMAGE:
			view->DrawBorder3D(x,y,w,h,m_style,3, HTMLFrame::LOOK_UP);
			break;
		case T_RADIO:
		case T_CHECKBOX:
			{
			HTMLFrame::Look look;
			if (m_activated)
				look = HTMLFrame::LOOK_DOWN;
			else
				look = HTMLFrame::LOOK_UP;
			view->DrawBorder3D(x,y+2,w-2,h-2,m_style,3, look);
			}
			break;
		case T_TEXT:
		case T_PASSWORD:
			view->DrawBorder3D(x,y,w,h,m_style,3, HTMLFrame::LOOK_DOWN);
			if (!strnull(m_value)) {
				view->DrawString(x+9,y+h-6,w-20,m_value,m_style);
			}
			break;
		case T_HIDDEN:
			break;
		case T_BOGGUS:
		default:
			view->DrawBorder3D(x,y,w,h,m_style,3, HTMLFrame::LOOK_UP);
			break;
	}
}

void OPTION_DocElem::RelationSet(HTMLFrame *view) {
	m_hidden = m_included;
	m_included = NULL;

	DocWalker walk(m_hidden);
	DocElem *elem;
	while ((elem = walk.Next()) != NULL) {
		walk.Feed(elem);
		StrDocElem *textElem;
		if ((textElem = dynamic_cast<StrDocElem *>(elem))) {
			strcat(m_text, textElem->str);
			strcat(m_text, " ");
		}
	}
	TagDocElem::RelationSet(view);
}

void OPTION_DocElem::geometry(HTMLFrame */*view*/) { 
	for (TagAttr *iter = list; iter!=NULL; iter=iter->Next()) {
		iter->ReadStrl("value",m_value, sizeof m_value);
		if (!strcasecmp(iter->AttrValue(), "selected")) {
			m_selected = true;
		}
	}
}