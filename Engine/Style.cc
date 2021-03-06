#include "Style.h"

const StyleMgr StyleMgr::DefaultStyleMgr;

// NEXUS/TODO: Introduce relative sizes. New field "rsize_", having 0 value will mean that the size 
// is absolute, otherwise should be relative. New methods: int RSize() and void RSize(int).
// Tags like H1-6 should set relative size to according positive or negative value

Style::Style() : 
		face_(B_REGULAR_FACE),
		size_(0)
	 {
	color = kBlack;
	bgcolor = kWhite;
	linkcolor = kLinkColor;
	isLink = false;
	isUnderline = false;
	ownerId = 0;
	m_indent = 0;
	m_nobr = false;
	m_alignment = al_bottom;
	m_marginTop = 0;
	m_marginBottom = 0;
	m_marginLeft = 0;
	m_marginRight = 0;
}

Style::Style(const Style &c) {
	color = c.color;
	bgcolor = c.bgcolor;
	linkcolor = c.linkcolor;
	isLink = c.isLink;
	isUnderline = c.isUnderline;
	m_indent = c.m_indent;
	m_nobr = c.m_nobr;
	face_ = c.face_;
	size_ = c.size_;
	m_alignment = c.m_alignment;

	// No inherit
	m_marginTop =0;
	m_marginBottom =0;
	m_marginLeft =0;
	m_marginRight =0;
}

void Style::SetLink() {
	isLink = true;
}

bool Style::IsLink() const {
	return isLink;
}

void Style::SetFace(uint16 f) {
	if (face_ == B_REGULAR_FACE)
		face_ =f;
	else
		face_ |= f;

}
void Style::SetUnderline() {
	isUnderline = true;
}

bool Style::IsUnderline() const {
	return isUnderline;
}

void Style::IncIndent(short marginLeft) {
	m_marginLeft = marginLeft;
	m_indent += marginLeft;
}

short Style::Indent() const {
	return m_indent;
}

void Style::SetAlignment(Alignment al) {
	m_alignment = al;
}

Alignment Style::Align() const {
	return m_alignment;
}

static char styleString[50];

const char *Style::toString() const {
	//sprintf(styleString,"/C:%6x/B:%6x",*(int*)&color,*(int*)&bgcolor);
	sprintf(styleString,"tId=%d\t",ownerId);
	return styleString;
}

const rgb_color kWhite = {255,255,255,0}; 
const rgb_color kLightGray = {235,235,235,0}; 
const rgb_color kGray = {200,200,200,0}; 
const rgb_color kDarkGray = {128,128,128,0}; 
const rgb_color kBlack = {0,0,0,0}; 

const rgb_color kLinkColor = {0,0,255,0}; 

const rgb_color kRed = {255,0,0,0}; 
const rgb_color kDarkRed = {128,0,0,0}; 

const rgb_color kGreen = {0,0x80,0,0}; 

const rgb_color kBlue = {0,0,255,0}; 
const rgb_color kLightBlue =  { 0xa0, 0xa0, 0xFF, 0 };

const rgb_color kYellow = {255,255,0,0}; 
const rgb_color kMagenta = {255,0,255,0}; 
