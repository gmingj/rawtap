
/* #include <string> */
#include <net/ethernet.h>

struct FrerHeader {
    struct ether_header eh;
    uint16_t reserved;
    uint16_t seqnum;
} __attribute__ ((__packed__));

class Net {
    private:
        int OpenSocket(const char *name);

    public:
        int rawsock_;
        /* unsigned int instanceid_; */

        Net();
        ~Net();

        int NetInit(std::string dev, std::string macaddr);
        /* TxLocalToFrer(); */
        int CreateFrerIns();
        void ProcessFrer();
        void TxFromFrerToLocal(char *buf, int len);
};
