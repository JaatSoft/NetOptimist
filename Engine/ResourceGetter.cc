#include "ResourceGetter.h"
#include "Url.h"
#include "UrlQuery.h"
#include "Resource.h"
#include <be/app/Message.h>
#include <stdio.h>


ResourceGetter::ResourceGetter(BLooper *notify) : BLooper("Http worker") {
	m_notify = notify;
}

void ResourceGetter::MessageReceived(BMessage *msg) {
	const char *urlStr = NULL;
	Url *url = NULL;
	void *ptr;
	switch(msg->what) {
		case GET_IMAGE:
			msg->FindString("url", &urlStr);
			msg->FindPointer("urlObject", &ptr);
			url = static_cast<Url*>(ptr);
			if (url) {
				if (urlStr) {
					fprintf(stderr, "ResourceGetter: got work : url %s\n", urlStr);
				}
				Resource *rsc;
				rsc = url->GetDataNow(); // This call MUST be synchronous !!!
				if (rsc) {
					if (!rsc->Data() && !rsc->CachedFile()) {
						fprintf(stderr, "ERROR ResourceGetter: url %s downloaded but no data\n", urlStr);
					}
				} else {
					fprintf(stderr, "ResourceGetter: failed for url %s\n", urlStr);
				}
				BLooper *notify = NULL;
				if (msg->FindPointer("notify", &ptr)==B_OK) {
					notify = static_cast<BLooper*>(ptr);
				}
				if (!notify)
					notify = m_notify;
				if (notify) {
					bool reformat = true;
					msg->FindBool("reformat", &reformat);
					BMessage msgNotifyEnd(URL_LOADED);
					msgNotifyEnd.AddBool("reformat", reformat);
					notify->PostMessage(&msgNotifyEnd);
				} else {
					fprintf(stderr, "ResourceGetter: cannot notify download completed\n");
				}
			} else {
				fprintf(stderr, "ResourceGetter: no url to download\n");
				abort();
			}
			break;
		default:
			fprintf(stderr, "Unknown message for ResourceGetter::MessageReceived %s\n", (char*)&msg->what);
	}
}

