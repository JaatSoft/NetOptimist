#ifndef ResourceGetter_H
#define ResourceGetter_H

#ifdef __BEOS__
 #include <OS.h>
#else
 #include "Be.h"
#endif
#include <be/app/Looper.h>

const int GET_IMAGE = 'GIMG';
const int REFRESH='RFSH';
const int REFORMAT='RFMT';


class ResourceGetter : public BLooper {
	BLooper *m_notify;
public:
	ResourceGetter(BLooper *notify);
	void MessageReceived(BMessage *msg);
};

#endif