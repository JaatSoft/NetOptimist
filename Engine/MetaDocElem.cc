#include "MetaDocElem.h"
#include "Style.h"
#include "UrlQuery.h"
#include "Url.h"
#include "Resource.h"
#include <Bitmap.h>
#include <be/translation/TranslationUtils.h>

bool MetaDocElem::IsActive() {
	return !strnull(m_url.Str());
}

bool MetaDocElem::Action(::Action /*action*/, UrlQuery *href) {
	href->SetUrl(&m_url);
	return href->Url() != NULL;
}


/*
NEXUS/TODO: 
1. Needs rewriting, so every meta element gets read and used/passed to the view.
2. Content-Type should be passed to view->SetContentType().
3. If there is no Content-Type specified, the one passed in HTTP response should be used, if still none is there
it should be assumed to octet-stream for HTTP/FTP protocols and should be read from the NodeInfo if 
protocol is FILE.
4. Charset should be passed to view->SetCharset(), so when "Auto-detection" of encoding is enabled
this value will be used.
5. Need to handle http-equiv="Set-Cookie".
6. There are also DESCRIPTION, KEYWORDS and other meta tags...
 */ 
void MetaDocElem::geometry(HTMLFrame *view) {
	StrRef httpEquivRef;
	StrRef contentRef;
	TagDocElem::geometry(view);
	// FIXME: will work for only one http-equiv string, i suppose. 
	for (TagAttr *iter = list; iter!=NULL; iter=iter->Next()) {
		iter->ReadStrRef("CONTENT" ,&contentRef);
		iter->ReadStrRef("HTTP-EQUIV" ,&httpEquivRef);
	}
	if (!httpEquivRef.IsFree()) {
		// FIXME: there is also Time settings which goes before ";url=", the user should be
		// redirected after this time has passed since this tag read
		printf("META : %s -> %s\n", httpEquivRef.Str(), contentRef.Str());
		if (!strcasecmp("refresh", httpEquivRef.Str())) {
			const char *content = contentRef.Str();
			while(content && *content) {
				int r = strcaseprefix(content, "url=");
				if (r>0) {
					printf("META URL=%s\n", content+r);
					m_url.SetToDup(content+r);
					break;
				}
				content++;
			}
		}

		// FIXME: Content-Type value is being skipped right now and not being read/saved
		StrRef ref;
		if (!strcasecmp("content-type", httpEquivRef.Str())) {
			const char *content = contentRef.Str();
			while(content && *content) {
				int r = strcaseprefix(content, "charset=");
				if (r>0) {
					printf("META charset=%s\n", content+r);
					ref.SetToDup(content+r);
					break;
				}
				content++;
			}
			
		}
	}
	// FIXME:
	if (!m_url.IsFree()) {
		char *str = new char[strlen(m_url.Str()) + 30];
	
		sprintf(str, "[Redirect: %s]", m_url.Str());
		m_text.SetToString(str, false);
		view->StringDim(str, m_style,&w,&h);
		w+=3; // XXX Some spaces between words : bad AND USELESS ?
		fixedW = w;
	
		m_includedStyle = m_style->clone(id);
		m_includedStyle->SetColor(m_style->LinkColor());
		m_includedStyle->SetLink();
		m_includedStyle->SetUnderline();
	}
}

void MetaDocElem::draw(HTMLFrame *view, const BRect *, bool onlyIfChanged) {
	if (onlyIfChanged) return;
	if (!m_text.IsFree()) {
		view->DrawString(x,y+h-3,w-3,m_text.Str(),m_includedStyle); // XXX I don't really like the "-3" (see Also in "StrDocElem.cc")
	}
	printPosition("MetaDocElem::draw");
}

void BODY_DocElem::geometry(HTMLFrame *view) {
	TagDocElem::geometry(view);
	m_includedStyle = m_style->clone(id);
	for (TagAttr *iter = list; iter!=NULL; iter=iter->Next()) {
		rgb_color color;
		if (iter->ReadColor("bgcolor",&color))
			m_includedStyle->SetBGColor(color);
		if (iter->ReadColor("text",&color))
			m_includedStyle->SetColor(color);
		if (iter->ReadColor("link",&color))
			m_includedStyle->SetLinkColor(color);
	}
	view->SetFrameColor(m_includedStyle);
}

void BODY_DocElem::draw(HTMLFrame *view, const BRect * updateRect, bool onlyIfChanged) {
	if (onlyIfChanged) return;
	TagDocElem::draw(view, updateRect);
}

void LINK_DocElem::RelationSet(HTMLFrame *view) {
	StrRef link_name;
	StrRef rlink_name;
	StrRef url_attr;
	for (TagAttr *iter = list; iter!=NULL; iter=iter->Next()) {
		iter->ReadStrRef("REL", &link_name);
		iter->ReadStrRef("REV", &rlink_name);
		iter->ReadStrRef("HREF", &url_attr);
	}
	const char *favLinkName = "no name";
	if (!link_name.IsFree()) {
		favLinkName = link_name.Str();
	} else if (!rlink_name.IsFree()) {
		favLinkName = rlink_name.Str();
	}
	if (!strcasecmp(favLinkName,"icon")
	 || !strcasecmp(favLinkName,"shortcut icon")) {
		if (Pref::Default.ShowImages()) {
			m_iconUrl = new Url(url_attr.Str(), view->m_document.CurrentUrl(), true, false);
		}
	} else if (!strcasecmp(favLinkName,"stylesheet")) {
		// XXX This is currently ignored. We should download it then parse it
	} else {
		fprintf(stderr, "Link : %s to %s\n", favLinkName, url_attr.Str());
		view->m_document.AddLink(&link_name, &url_attr);
	}

	TagDocElem::RelationSet(view);
}

void LINK_DocElem::dynamicGeometry(HTMLFrame *view) {
	if (m_iconUrl) {
		Resource *r = m_iconUrl->GetIfAvail();
		BBitmap *bmp = NULL;
		if (r && r->CachedFile()) {
			bmp = BTranslationUtils::GetBitmapFile(r->CachedFile());
		}
		if (r && r->Data()) {
			bmp = BTranslationUtils::GetBitmap(r->Data());
		}
		view->m_document.SetIcon(bmp);

		delete m_iconUrl;
		m_iconUrl = NULL;
	}
	super::dynamicGeometry(view);
}
