#include <pthread.h>
#include <net/if.h>
#include <string>

class Control {
    public:
	    pthread_t ctrltid_;
	    int ctrlsock_;
	    int instanceid_;
        int instancenum_;

	    pthread_t taptid_;
	    int tapsock_;
        char tapname_[IFNAMSIZ];

        Control() {};
        ~Control() {};
        
        void CreateThread(void);
        int CreateCtrlSock(std::string domain);
        void CreateTapInsThread(void);
};
