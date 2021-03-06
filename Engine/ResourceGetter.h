#ifndef ResourceGetter_H
#define ResourceGetter_H

#if defined(__BEOS__) || defined(__HAIKU__)
 #include <OS.h>
#else
 #include "Be.h"
#endif
#include <be/app/Looper.h>

const int GET_IMAGE = 'GIMG';
const int URL_LOADED='ULDD';


class ResourceGetter : public BLooper {
	BLooper *m_notify;
public:
	ResourceGetter(BLooper *notify);
	void MessageReceived(BMessage *msg);
};

#endif
