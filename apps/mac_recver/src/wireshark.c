#include <stdarg.h>
#include <limits.h>

#include <os/os.h>
#include <dpl/dpl.h>
#include <uart/uart.h>

#include <mac_recver/wireshark.h>

static struct uart_dev *g_uart_dev;
static on_cmd_t g_on_cmd;

static int
wireshark_uart_tx_char(void *arg)
{
    return 0;
}

static int
wireshark_uart_rx_char(void *arg, uint8_t byte)
{
    uart_blocking_tx(g_uart_dev, byte);

    static char cmd_buff[MYNEWT_VAL(CMD_BUFF_SIZE)];
    static uint8_t g_idx=0;
    int channel;

    if(byte != '\r' && byte != '\n' && g_idx < MYNEWT_VAL(CMD_BUFF_SIZE)){
        cmd_buff[g_idx] = byte;
        g_idx++;
    }
    else if(byte == '\r' || byte == '\n'){
        cmd_buff[g_idx] = 0;
        g_idx = 0;
        if(g_on_cmd != NULL){
            if(sscanf(cmd_buff, "channel %d", &channel)){
                g_on_cmd(CMD_CHANNEL, (void*)channel);
            }
            else if(!strcmp(cmd_buff, "receive")){
                g_on_cmd(CMD_RECV, 0);
            }
            else if(!strcmp(cmd_buff, "sleep")){
                g_on_cmd(CMD_SLEEP, 0);
            }
        }
    }
    return 0;
}

int wireshark_init(on_cmd_t on_cmd){
    struct uart_conf uc = {
        .uc_speed = MYNEWT_VAL(COM_UART_BAUD),
        .uc_databits = 8,
        .uc_stopbits = 1,
        .uc_parity = UART_PARITY_NONE,
        .uc_flow_ctl = UART_FLOW_CTL_NONE,
        .uc_tx_char = wireshark_uart_tx_char,
        .uc_rx_char = wireshark_uart_rx_char,
    };
    g_uart_dev = (struct uart_dev *)os_dev_open(MYNEWT_VAL(COM_UART_DEV),
        OS_TIMEOUT_NEVER, &uc);
    if (!g_uart_dev) {
        printf("UART device: not found");
        return -1;
    }
    g_on_cmd = on_cmd;

    return 0;
}

struct param {
    unsigned char width; /**< field width */
    char lz;            /**< Leading zeros */
    char sign:1;        /**<  The sign to display (if any) */
    char alt:1;         /**< alternate form */
    char uc:1;          /**<  Upper case (for base16 only) */
    char left:1;        /**<  Force text to left (padding on right) */
    char base;  /**<  number base (e.g.: 8, 10, 16) */
    char *bf;           /**<  Buffer to output */
};

static void 
ui2a(unsigned long long int num, struct param *p)
{
    int n = 0;
    unsigned long long int d = 1;
    char *bf = p->bf;
    while (num / d >= p->base)
        d *= p->base;
    while (d != 0) {
        unsigned long long  dgt = num / d;
        num %= d;
        d /= p->base;
        if (n || dgt > 0 || d == 0) {
            *bf++ = dgt + (dgt < 10 ? '0' : (p->uc ? 'A' : 'a') - 10);
            ++n;
        }
    }
    *bf = 0;
}

static void 
i2a(long long int num, struct param *p)
{
    if (num < 0) {
        num = -num;
        p->sign = 1;
    }
    ui2a(num, p);
}

static unsigned long long
intarg(int lng, int sign, va_list *va)
{
    unsigned long long val;

    switch (lng) {
    case 0:
        if (sign) {
            val = va_arg(*va, int);
        } else {
            val = va_arg(*va, unsigned int);
        }
        break;

    case 1:
        if (sign) {
            val = va_arg(*va, long);
        } else {
            val = va_arg(*va, unsigned long);
        }
        break;

    case 2:
    default:
        if (sign) {
            val = va_arg(*va, long long);
        } else {
            val = va_arg(*va, unsigned long long);
        }
        break;
    }

    return val;
}

static int 
a2d(char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    else if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    else if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    else
        return -1;
}

static char 
a2i(char ch, const char **src, int base, unsigned char *nump)
{
    const char *p = *src;
    int num = 0;
    int digit;
    while ((digit = a2d(ch)) >= 0) {
        if (digit > base)
            break;
        num = num * base + digit;
        ch = *p++;
    }
    *src = p;
    *nump = num;
    return ch;
}

static int 
putf(char c)
{
    uart_blocking_tx(g_uart_dev, c);
    return 1;
}

static unsigned 
putchw(struct param *p)
{
    unsigned written = 0;
    char ch;
    int n = p->width;
    char *bf = p->bf;

    /* Number of filling characters */
    while (*bf++ && n > 0)
        n--;
    if (p->sign)
        n--;
    if (p->alt && p->base == 16)
        n -= 2;
    else if (p->alt && p->base == 8)
        n--;

    /* Unless left-aligned, fill with space, before alternate or sign */
    if (!p->lz && !p->left) {
        while (n-- > 0)
            written += putf(' ');
    }

    /* print sign */
    if (p->sign)
        written += putf('-');

    /* Alternate */
    if (p->alt && p->base == 16) {
        written += putf('0');
        written += putf((p->uc ? 'X' : 'x'));
    } else if (p->alt && p->base == 8) {
        written += putf('0');
    }

    /* Fill with zeros, after alternate or sign */
    if (p->lz) {
        while (n-- > 0)
            written += putf('0');
    }

    /* Put actual buffer */
    bf = p->bf;
    while ((ch = *bf++))
        written += putf(ch);

    /* If left-aligned, pad the end with spaces. */
    if (p->left) {
        while (n-- > 0)
            written += putf(' ');
    }
    
    return written;
}

static size_t 
tfp_format(const char *fmt, va_list va)
{
    size_t written = 0;
    struct param p;
    char bf[23];
    char ch;
    char lng;
    void *v;
    int i;

    p.bf = bf;

    while ((ch = *(fmt++))) {
        if (ch != '%') {
            written += putf(ch);
        } else {
            /* Init parameter struct */
            p.lz = 0;
            p.alt = 0;
            p.width = 0;
            p.sign = 0;
            p.left = 0;
            p.uc = 0;
            lng = 0;

            /* Flags */
            while ((ch = *(fmt++))) {
                switch (ch) {
                case '0':
                    if (!p.left) {
                        p.lz = 1;
                    }
                    continue;
                case '#':
                    p.alt = 1;
                    continue;
                case '-':
                    p.left = 1;
                    p.lz = 0;
                    continue;
                default:
                    break;
                }
                break;
            }

            /* Width */
            if (ch == '*') {
                i = intarg(0, 1, &va);
                if (i > UCHAR_MAX) {
                    p.width = UCHAR_MAX;
                } else if (i > 0) {
                    p.width = i;
                }
                ch = *(fmt++);
            } else if (ch >= '0' && ch <= '9') {
                ch = a2i(ch, &fmt, 10, &(p.width));
            }
            if (ch == 'l') {
                ch = *(fmt++);
                lng = 1;

                if (ch == 'l') {
                    ch = *(fmt++);
                    lng = 2;
                }
            }

            if (ch == 'z') {
                ch = *(fmt++);
            }

            switch (ch) {
            case 0:
                goto abort;
            case 'u':
                p.base = 10;
                ui2a(intarg(lng, 0, &va), &p);
                written += putchw(&p);
                break;
            case 'd':
            case 'i':
                p.base = 10;
                i2a(intarg(lng, 1, &va), &p);
                written += putchw(&p);
                break;
            case 'x':
            case 'X':
                p.base = 16;
                p.uc = (ch == 'X');
                ui2a(intarg(lng, 0, &va), &p);
                written += putchw(&p);
                break;
            case 'o':
                p.base = 8;
                ui2a(intarg(lng, 0, &va), &p);
                written += putchw(&p);
                break;
            case 'p':
                v = va_arg(va, void *);
                p.base = 16;
                ui2a((uintptr_t)v, &p);
                p.width = 2 * sizeof(void*);
                p.lz = 1;
                written += putf('0');
                written += putf('x');
                written += putchw(&p);
                break;
            case 'c':
                written += putf((char)(va_arg(va, int)));
                break;
            case 's':
                p.bf = va_arg(va, char *);
                written += putchw(&p);
                p.bf = bf;
                break;
            case '%':
                written += putf(ch);
                break;
            default:
                break;
            }
        }
    }
 abort:;
 
 return written;
}

int wireshark_printf(const char *fmt, ...){
    if(g_uart_dev == NULL) return 0;
    va_list va;
    va_start(va, fmt);
    int rv = tfp_format(fmt, va);
    va_end(va);
    return rv;
}
