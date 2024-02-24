
/* C interface for HDPMI vendor API */

struct HDPMIAPI_ENTRY {
    uint32_t ofs;
    uint16_t seg;
};

extern uint8_t _get_hdpmi_vendor_api( struct HDPMIAPI_ENTRY * );

extern uint8_t _hdpmi_set_context_mode( uint8_t bMode );

/* struct expected by HDPMI port trapping API */

struct __attribute__((packed)) _hdpmi_traphandler {
    uint32_t ofsIn;
    uint16_t segIn;
    uint32_t ofsOut;
    uint16_t segOut;
};

extern uint32_t _hdpmi_install_trap( int port, int cnt, struct _hdpmi_traphandler * );
extern bool     _hdpmi_uninstall_trap( uint32_t handle );
extern uint8_t  _hdpmi_simulate_byte_in( int port );
extern void     _hdpmi_simulate_byte_out( int port, uint8_t value );
extern void     _hdpmi_set_cli_handler( void (* hdlproc)(void) );
