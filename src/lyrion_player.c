#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>

#include <sys/types.h>
#include <string.h>
#include <ctype.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netdb.h>

#include "timing.h"
#include "logging.h"
#include "lyrion_player.h"
#define PORT 9090

/*
 * Default.map
##################################################################################
# Button names to function mappings
##################################################################################
# The format here is buttonname = function, grouped by [modename]

[common]
# Button functions that are common to all modes (unless overridden)
<snip>
0.hold					= playFavorite_0
1.hold					= playFavorite_1
2.hold					= playFavorite_2
3.hold					= playFavorite_3
4.hold					= playFavorite_4
5.hold					= playFavorite_5
6.hold					= playFavorite_6
7.hold					= playFavorite_7
8.hold					= playFavorite_8
9.hold					= playFavorite_9

<SNIP>
rew.single				= jump_rew
rew.hold				= scan_rew

<SNIP>
fwd.single				= jump_fwd
fwd.hold				= scan_fwd

<SNIP>
muting					= muting
pause.single			= pause
pause.hold				= stop
play.single				= play
play.hold				= play
power					= power_toggle
power_off				= power_off
power_on				= power_on

<SNIP>
sleep					= sleep
stop					= stop
voldown					= volume_down
voldown.repeat			= volume_down
volup					= volume_up
volup.repeat			= volume_up
<SNIP>

# shuffle modes
shuffle					= shuffle_toggle
shuffle_off				= shuffle_off
shuffle_on				= shuffle_on

# repeat modes
repeat					= repeat_toggle
repeat_off				= repeat_0
repeat_one				= repeat_1
repeat_all				= repeat_2
<SNIP>

 */
#define SET_READONLY_CHAR_PTR(x, v) *(const char **)(&(x)) = (v)

int strcmp_ex(const char* const x, const char* const y) {
    if (x == y) return 0;
    if (x == NULL) return -1;
    if (y == NULL) return 1;
    return strcmp(x, y);
}

typedef enum {
    BIT_INDEX_player_name,
    BIT_INDEX_player_connected,
    BIT_INDEX_player_ip,
    BIT_INDEX_power,
    BIT_INDEX_signalstrength,
    BIT_INDEX_mode,
    BIT_INDEX_remote,
    BIT_INDEX_current_title,
    BIT_INDEX_time,
    BIT_INDEX_rate,
    BIT_INDEX_duration,
    BIT_INDEX_sync_master,
    BIT_INDEX_sync_slaves,
    BIT_INDEX_mixer_volume,
    BIT_INDEX_playlist_repeat,
    BIT_INDEX_playlist_shuffle,
    BIT_INDEX_playlist_duration,
    BIT_INDEX_playlist_mode,
    BIT_INDEX_seq_no,
    BIT_INDEX_playlist_cur_index,
    BIT_INDEX_playlist_timestamp,
    BIT_INDEX_playlist_tracks,
    BIT_INDEX_randomplay,
    BIT_INDEX_digital_volume_control,
    BIT_INDEX_use_volume_control,
    BIT_INDEX_remoteMeta,
    BIT_INDEX_playlist_index,
    BIT_INDEX_id,
    BIT_INDEX_title,
    BIT_INDEX_album,
    BIT_INDEX_type,
    BIT_INDEX_bitrate,
    BIT_INDEX_year,
    BIT_INDEX_composer,
    BIT_INDEX_conductor,
    BIT_INDEX_band,
    BIT_INDEX_albumartist,
    BIT_INDEX_trackartist,
    BIT_INDEX_release_type,
    BIT_INDEX_filesize,
    BIT_INDEX_genre,
    BIT_INDEX_genres,
    BIT_INDEX_disc,
    BIT_INDEX_samplesize,
    BIT_INDEX_bpm,
    BIT_INDEX_remote_title,
    BIT_INDEX_lossless,
    BIT_INDEX_disccount,
    BIT_INDEX_tracknum,
    BIT_INDEX_samplerate,
    BIT_INDEX_album_replay_gain,
    BIT_INDEX_replay_gain,
    BIT_INDEX_subtitle,
    BIT_INDEX_artist,
    BIT_INDEX_compilation,
    BIT_INDEX_can_seek,
    BIT_INDEX_waitingToPlay,
    BIT_INDEX_isClassical,

    BIT_INDEX_END
}bit_index;

typedef enum {
    NULL_KEY = 0,
    player_name = 0x034040e88, 
    player_connected = 0x032babfae, 
    player_ip = 0x00b7254f4, 
    power = 0x001004525, 
    signalstrength = 0x028d6de2a, 
    mode = 0x0000256bd, 
    remote = 0x009a9143a, 
    current_title = 0x032d6e841, 
    elapsed_time = 0x0000277d3,   // hash is for string "time", use elapsed_time to avoid symbol clashes
    rate = 0x000029120, 
    duration = 0x02d9e012e, 
    sync_master = 0x026c55332, 
    sync_slaves = 0x018ff77d3, 
    mixer_volume = 0x03083387c, 
    playlist_repeat = 0x01f776c47, 
    playlist_shuffle = 0x029fb26ae, 
    playlist_duration = 0x029eed1b2,
    playlist_mode = 0x01d83eb8e, 
    seq_no = 0x007d219fb, 
    playlist_cur_index = 0x03b7f7ad5, 
    playlist_timestamp = 0x0107115de, 
    playlist_tracks = 0x02986f8af, 
    randomplay = 0x00a88085d, 
    digital_volume_control = 0x014fb2edd, 
    use_volume_control = 0x02318519a, 
    remoteMeta = 0x03b3e7808, 
    playlist_index = 0x00d0d0d4b, 
    id = 0x000000085, 
    title = 0x0004c3638, 
    album = 0x000c0c64f, 
    type = 0x000028506, 
    bitrate = 0x000b0b4e0, 
    year = 0x000083323, 
    composer = 0x00d48a34c,
    conductor = 0x022c368b1,
    band = 0x00002062b,
    albumartist = 0x02983929e,
    trackartist = 0x0295f31fa,
    release_type = 0x026cc2bc9,
    filesize = 0x036736f83,
    genre = 0x0004ed963,
    genres = 0x020baed30,
    disc = 0x00001a58b,
    samplesize = 0x03612c300,
    bpm = 0x0000032bf,
    remote_title = 0x015e4dd41,
    lossless = 0x02d1f9ad1,
    disccount = 0x003ff9814,
    tracknum = 0x02a79a4cd,
    samplerate = 0x02221466b,
    album_replay_gain = 0x01b8b79db,
    replay_gain = 0x01e5ddb06,
    subtitle = 0x02f5cbbb4,
    artist = 0x023310f99,
    compilation = 0x02cfbc9da,
    can_seek = 0x0372ea493,
    waitingToPlay = 0x02e3ef240,
    isClassical = 0x0157b941d,

    playlist_name = 0x014b00b23,
    playlist_modified = 0x030b9954c,
    playlist_id = 0x03584a535,

    VOLUME = 0x022fa5670,
    ARTIST = 0x013d3211e,
    TITLE = 0x027895b9d,
    PLAYLIST_CURRENT = 0x008374ad5,
    REMOTE_CHAR = 0x02a0a4c63,
    REMOTE = 0x0235a4344,
    COMPILATION_CHAR = 0x00816a834,
    COMPILATION = 0x02e3a9e93,
    GENRE = 0x0278bfec8,
    GENRES = 0x0115cfeb5,
    LOSSLESS = 0x02d6bcec8,    
    REPEAT = 0x012d3980a,
    SHUFFLE = 0x030bd9e9b,
    WAITING = 0x026eea81d,
    RANDOMPLAY = 0x01b2ee5e9,
    MODE = 0x029026c42,
    CAN_FWD = 0x00ea7f446,
    CAN_REW = 0x01177ebe3,
    DURATION = 0x004db17a0,
    CAN_SEEK = 0x00e7a46e5,
    YEAR = 0x0290848a8,
    ALBUM_OR_REMOTE_TITLE = 0x007d55ca0,
    BITRATE = 0x0028894c6,
    PLAYLIST_TRACKS = 0x03715454c,
}player_key_hashv;

typedef struct {
        const char* const  status_buffer;
        int     playlist_cur_index;
        int     playlist_index;
        int     year;
        int     samplerate;
        int     samplesize;
        int     lossless;
        int     elapsed_time;
        int     duration;
        int     tracknum;
        int     remote;
        int     compilation;
        int     can_seek;
        int     disc;
        int     disccount;
        int     randomplay;
        int     playlist_tracks;
        int     filesize;
        int     playlist_repeat;
        int     playlist_shuffle;
        int     playlist_duration;
        int     id;
        int     waitingToPlay;
        int     isClassical;

        const char* const   bitrate;
        const char* const   current_title;
        const char* const   title;
        const char* const   artist;
        const char* const   albumartist;
        const char* const   trackartist;
        const char* const   composer;
        const char* const   conductor;
        const char* const   band;
        const char* const   album;
        const char* const   type;
        const char* const   replay_gain;
        const char* const   album_replay_gain;
        const char* const   genre;
        const char* const   genres;
        const char* const   subtitle;
        const char* const   release_type;
        const char* const   remote_title;
        const char* const   mode;

        const char* const   meta_artist;

        unsigned char field_set[(BIT_INDEX_END+8)/8];

}player_status, *player_status_ptr;

typedef struct local_addr* local_addr_ptr;
typedef struct local_addr {
    local_addr_ptr next;
    const char* addr;
}local_addr;

typedef struct {
    char cmd_buff[1024];
    char buffer[4096];
    int  sockfd;
    FILE *fp;
    bool lock;
}lms_io, *lms_io_ptr;

struct lyrion_player {
    const char* lms;
    local_addr_ptr lp;
    const char* lms_player_id;
    lms_io io;
    lms_io cmd_io;
    player_status status;
    int volume;
};

static void lock_io(lms_io_ptr iop) {
    while(__atomic_test_and_set(&iop->lock, __ATOMIC_ACQ_REL)) {
        sleep_milli_seconds(1);
    }
}

static void unlock_io(lms_io_ptr iop) {
    __atomic_clear(&iop->lock, __ATOMIC_RELEASE);
}

static inline void free_ex(void** tgt) {
    if (*tgt) {
        free(*tgt);
    }
    *tgt = NULL;
}

#define FREE(x) free_ex((void **)(&x))
#define ZERO(x) memset((x), 0, sizeof(*(x)))

uint64_t compute_player_hash(const char* s) {
    const int p = 31;
    const int m = 1e9 + 9;
    uint64_t hash_value = 0;
    uint64_t p_pow = 1;
    char *c = (char *)s;
    while(*c) {
        hash_value = (hash_value + (*c - 'a' + 1) * p_pow) % m;
        p_pow = (p_pow * p) % m;
        ++c;
    }
    return hash_value;
}

static void get_local_addrs(player_ptr player) {
    struct ifaddrs *ifaddr;
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
//        exit(EXIT_FAILURE);
        return;
    }
    for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) {
            continue;
        }
        if (ifa->ifa_addr->sa_family == AF_INET || ifa->ifa_addr->sa_family == AF_INET6) {
            char host[NI_MAXHOST];
            int s = getnameinfo(ifa->ifa_addr,
                        (ifa->ifa_addr->sa_family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6),
                        host, NI_MAXHOST,
                        NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                error_printf("getnameinfo() failed: %s\n", gai_strerror(s));
                exit(EXIT_FAILURE);
            }

            local_addr_ptr lp = calloc(1, sizeof(*lp) + strlen(host) + 4);
            if (lp) {
                lp->addr = (char *)(lp + 1);
                strcpy((char *)lp->addr, host);
                lp->next = player->lp;
                player->lp = lp;
            }
        } 
    }
    freeifaddrs(ifaddr);    
}

// convert %NN to chars and spaces to new lines
static void deescape(char *buff) {
    char* dst=buff;
    char* src=buff;

    while(*src) {
        *dst = *src;
        switch(*src) {
            case '%':
                if (isxdigit(*(src+1)) && isxdigit(*(src+2))) {
                    unsigned v;
                    sscanf(src + 1, "%02x", &v);
                    *dst = (char)v;
                    src += 2;
                }
                break;
            case ' ':
                *dst = '\n';
                break;
            default:
                break;
        }
        src++;
        dst++;
    }
    *dst=0;
}

static char* splitn(char* in, char **next) {
    char *c = in;
    *next = NULL;
    while(*c) {
        if (*c == '\n') {
            *c = 0;
            *next = c+1;
            break;
        }
        ++c;
    }
    return in;
}

static char* splitcolon(char* in) {
    char *c = in;
    while(*c) {
        if (*c == ':') {
            *c = 0;
            return c+1;
        }
        ++c;
    }
    return NULL;
}

static int escaped_strlen(const char* str) {
    const char *p = str;
    int len = 0;
    while(*p) {
        ++len;
        if (*p == ':') {
            len += 2;
        }
        p++;
    }
    return len;
}

static char* req0(player_ptr player, const char* prefix, const char* suffix, lms_io_ptr io_ptr, const char *format, va_list args) {
    char *rv = NULL;
    lock_io(io_ptr);
    ZERO(io_ptr->cmd_buff);
    ZERO(io_ptr->buffer);
    char *p = io_ptr->cmd_buff;
    size_t len = sizeof(io_ptr->cmd_buff);
    if (prefix) {
        int prefixlen = strlen(prefix);
        strcpy(p, prefix);
        p[prefixlen] = ' ';
        p += prefixlen + 1;
        len -= prefixlen;
    }
    int w = vsnprintf(p, len, format, args);
    int suffixlen = strlen(suffix);
    if (w < len - suffixlen - 1) {
        strcat(p, suffix);
        int len = strlen(io_ptr->cmd_buff);
        if ( 0 < send(io_ptr->sockfd, io_ptr->cmd_buff, len, 0))
        {
            fgets(io_ptr->buffer, sizeof(io_ptr->buffer), io_ptr->fp);
            int hdr_len = escaped_strlen(io_ptr->cmd_buff) - suffixlen + 1;
            char* c = io_ptr->buffer + hdr_len;
            while (*c != 0) {
                if (*c == '\n' || *c == '\r') {
                    *c = 0;
                    break;
                }
                ++c;
            }
            rv = strdup(io_ptr->buffer + hdr_len);
        } else {
            error_printf("failed to send data over socket\n");
        }
    } else {
        fprintf(stderr, "cmd buff is too small!\n%s\n", io_ptr->cmd_buff);
    }
    unlock_io(io_ptr);
    return rv;
}


static char* lms_query(player_ptr player, const char *format, ...) {
    va_list args;
    va_start(args, format);
    char* rv = req0(player, NULL, " ?\n", &player->io, format, args);
    va_end(args);
    return rv;
}

static char* lms_query_player(player_ptr player, const char *format, ...) {
    va_list args;
    va_start(args, format);
    char* rv = req0(player, player->lms_player_id, " ?\n", &player->io, format, args);
    va_end(args);
    return rv;
}

static char* lms_compound_query_player(player_ptr player, const char *format, ...) {
    va_list args;
    va_start(args, format);
    char* rv = req0(player, player->lms_player_id, "\n", &player->io, format, args);
    va_end(args);
    return rv;
}

static void lms_command(player_ptr player, const char *format, ...) {
    va_list args;
    va_start(args, format);
    char* rv = req0(player, player->lms_player_id, "\n", &player->cmd_io, format, args);
    va_end(args);
    FREE(rv);
}

static void clear_player_status(player_status_ptr status_ptr) {
    if(status_ptr->meta_artist) {
        FREE(status_ptr->meta_artist);
    }
    if(status_ptr->status_buffer) {
        FREE(status_ptr->status_buffer);
    }
    ZERO(status_ptr);
 
    status_ptr->playlist_cur_index = -1;
    status_ptr->playlist_index = -1;
    status_ptr->year = -1;
    status_ptr->samplerate = -1;
    status_ptr->samplesize = -1;
    status_ptr->lossless = -1;
    status_ptr->elapsed_time = -1;
    status_ptr->duration = -1;
    status_ptr->tracknum = -1;
    status_ptr->remote = -1;
    status_ptr->compilation = -1;
    status_ptr->can_seek = -1;
    status_ptr->disc = -1;
    status_ptr->disccount = -1;
    status_ptr->randomplay = -1;
    status_ptr->playlist_tracks = -1;
    status_ptr->filesize = -1;
    status_ptr->playlist_repeat = -1;
    status_ptr->playlist_shuffle = -1;
    status_ptr->playlist_duration = -1;
    status_ptr->id = -1;
    status_ptr->waitingToPlay = -1;
    status_ptr->isClassical = -1;
}

static void get_player_volume(player_ptr player) {
    player->volume = -1;
    char *p = lms_query_player(player, "mixer volume");
    player->volume = atoi(p);
    FREE(p);
}

static bool get_player_status(player_ptr player) {
#define  SET_INTVALUE(nm) {\
    int _intval_ = atoi(value); \
    status_changed = status_changed || player->status.nm != _intval_; \
    status.field_set[BIT_INDEX_##nm/8] |= 1 << (BIT_INDEX_##nm%8);\
    status.nm = _intval_; \
    }
    
#define  SET_STRVALUE(nm) {\
    status_changed = status_changed || player->status.nm == NULL || strcmp(player->status.nm, value); \
    status.field_set[BIT_INDEX_##nm/8] |= 1 << (BIT_INDEX_##nm%8);\
    SET_READONLY_CHAR_PTR(status.nm, value); \
    }
    bool status_changed = false;
    int cur_index = player->status.playlist_cur_index;
    cur_index = cur_index < 0 ? 0 : cur_index;
    player_status status = {};
    ZERO(&status);
    do {
        char* p = lms_compound_query_player(player, "status %d 1 tags:aAACGNQliImoqrtyTXY duration", cur_index);
        deescape(p);
        clear_player_status(&status);
        SET_READONLY_CHAR_PTR(status.status_buffer, strdup(p));
        char *n = (char *)(status.status_buffer);
        while(n) {
            char *key = splitn(n, &n);
            char *value = splitcolon(key);
            player_key_hashv keyhashv = compute_player_hash(key);
//            printf("%s=0x%09lx\n", key, keyhashv);
            switch(keyhashv) {
                case elapsed_time:
                    // do not flag 
                    player->status.elapsed_time = status.elapsed_time = atoi(value);
                    break;
                case remote:
                    SET_INTVALUE(remote);
                    break;
                case compilation:
                    SET_INTVALUE(compilation);
                    break;
                case can_seek:
                    SET_INTVALUE(can_seek);
                    break;
                case duration:
                    SET_INTVALUE(duration);
                    break;
                case playlist_cur_index:
                    SET_INTVALUE(playlist_cur_index);
                    break;
                case playlist_tracks:
                    SET_INTVALUE(playlist_tracks);
                    break;
                case randomplay:
                    SET_INTVALUE(randomplay);
                    break;
                case playlist_index:
                    SET_INTVALUE(playlist_index);
                    break;
                case year:
                    SET_INTVALUE(year);
                    break;
                case filesize:
                    SET_INTVALUE(filesize);
                    break;
                case disc:
                    SET_INTVALUE(disc);
                    break;
                case samplesize:
                    SET_INTVALUE(samplesize);
                    break;
                case lossless:
                    SET_INTVALUE(lossless);
                    break;
                case disccount:
                    SET_INTVALUE(disccount);
                    break;
                case tracknum:
                    SET_INTVALUE(tracknum);
                    break;
                case samplerate:
                    SET_INTVALUE(samplerate);
                    break;
                case playlist_repeat:
                    SET_INTVALUE(playlist_repeat);
                    break;
                case playlist_shuffle: 
                    SET_INTVALUE(playlist_shuffle); 
                    break;
                case playlist_duration: 
                    SET_INTVALUE(playlist_duration); 
                    break;
                case id: 
                    SET_INTVALUE(id); 
                    break;
                case waitingToPlay:
                    SET_INTVALUE(waitingToPlay);
                    break;
                case isClassical:
                    SET_INTVALUE(isClassical);
                    break;

                case current_title:
                    SET_STRVALUE(current_title);
                    break;
                case title:
                    SET_STRVALUE(title);
                    break;
                case album:
                    SET_STRVALUE(album);
                    break;
                case type:
                    SET_STRVALUE(type);
                    break;
                case bitrate:
                    SET_STRVALUE(bitrate);
                    break;
                case composer:
                    SET_STRVALUE(composer);
                    break;
                case conductor:
                    SET_STRVALUE(conductor);
                    break;
                case band:
                    SET_STRVALUE(band);
                    break;
                case artist:
                    SET_STRVALUE(artist);
                    break;
                case albumartist:
                    SET_STRVALUE(albumartist);
                    break;
                case trackartist:
                    SET_STRVALUE(trackartist);
                    break;
                case release_type:
                    SET_STRVALUE(release_type);
                    break;
                case genre:
                    SET_STRVALUE(genre);
                    break;
                case genres:
                    SET_STRVALUE(genres);
                    break;
                case remote_title:
                    SET_STRVALUE(remote_title);
                    break;
                case album_replay_gain:
                    SET_STRVALUE(album_replay_gain);
                    break;
                case replay_gain:
                    SET_STRVALUE(replay_gain);
                    break;
                case subtitle:
                    SET_STRVALUE(subtitle);
                    break;
                case mode:
                    SET_STRVALUE(mode);
                    break;

                case player_name:
                case player_connected:
                case player_ip:
                case power:
                case signalstrength:
                case rate:
                case sync_master:
                case sync_slaves:
                case mixer_volume:
                case playlist_mode:
                case playlist_timestamp:
                case seq_no:
                case digital_volume_control:
                case use_volume_control:
                case remoteMeta:
                case bpm:

                case playlist_name:
                case playlist_modified:
                case playlist_id:
                    break;

                // pseudo tokens 
                case VOLUME:
                case ARTIST:
                case TITLE:
                case PLAYLIST_CURRENT:
                case REMOTE:
                case REMOTE_CHAR:
                case COMPILATION:
                case COMPILATION_CHAR:
                case GENRE:
                case GENRES:
                case LOSSLESS:
                case REPEAT:
                case SHUFFLE:
                case WAITING:
                case RANDOMPLAY:
                case MODE:
                case CAN_FWD:
                case CAN_REW:
                case DURATION:
                case CAN_SEEK:
                case YEAR:
                    break;
                
                case NULL_KEY:
                    break;
                default:
                    error_printf("get_player_status unhandled %s %s\n", key, value);
            }
        }
        cur_index = status.playlist_cur_index;
        FREE(p);
    } while(status.playlist_cur_index != status.playlist_index);
    for(int ix=0; status_changed == false && ix < sizeof(status.field_set); ++ix) {
        status_changed = status_changed || player->status.field_set[ix] != status.field_set[ix];
    }
    if (status_changed) {
        clear_player_status(&player->status);
        memcpy(&player->status, &status, sizeof(player_status));
        const char* artiste = player->status.artist ? player->status.artist : player->status.trackartist;
        if (artiste) {
            if (player->status.albumartist && strcmp(artiste, player->status.albumartist)) {
                char buffer[511];
                snprintf(buffer, sizeof(buffer), "%s, %s", player->status.albumartist, artiste);
                SET_READONLY_CHAR_PTR(player->status.meta_artist, strdup(buffer));
            }
        } else {
            if (player->status.albumartist) {
                SET_READONLY_CHAR_PTR(player->status.meta_artist, strdup(player->status.albumartist));
            }
        }
    } else {
        clear_player_status(&status);
    }
    return status_changed;
#undef SET_STRVALUE
#undef SET_INTVALUE
}

static void __close_player(player_ptr player) {
    if (player->io.fp) {
        fclose(player->io.fp);
        player->io.fp = NULL;
    }
    if (player->io.sockfd >= 0) {
        close(player->io.sockfd);
        player->io.sockfd = -1;
    }
    if (player->cmd_io.fp) {
        fclose(player->cmd_io.fp);
        player->cmd_io.fp = NULL;
    }
    if (player->cmd_io.sockfd >= 0) {
        close(player->cmd_io.sockfd);
        player->cmd_io.sockfd = -1;
    }
    local_addr_ptr lp = player->lp;
    while(lp) {
        player->lp = player->lp->next;
        free(lp);
        lp = player->lp;
    }
    if (player->lms_player_id) {
        FREE(player->lms_player_id);
    }
    clear_player_status(&player->status);
}

static int __open_player(player_ptr player) {
    player->io.sockfd = -1;
    player->cmd_io.sockfd = -1;

    get_local_addrs(player);

    struct sockaddr serv_addr;

    struct addrinfo hints;
    struct addrinfo *result, *rp;
    ZERO(&hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    int s = getaddrinfo(player->lms, "9090", &hints, &result);
    if (s != 0) {
        error_printf("getaddrinfo: %s\n", gai_strerror(s));
        __close_player(player);
        return -1;
    } 
    int serv_addr_set = 0;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        switch(rp->ai_family) {
            case AF_INET:
            case AF_INET6:
                memcpy(&serv_addr, rp->ai_addr, sizeof(serv_addr));
                serv_addr_set = 1;
                break;
            default:
                error_printf("ignoring address family %d\n", rp->ai_family);
                break;
        }
    }
    freeaddrinfo(result);
    if (!serv_addr_set) {
        error_printf("Failed to find server address\n");
        return -5;
    }
    if ((player->io.sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        error_printf("Socket creation error\n");
        return -4;
    }
    
    int status =  connect(player->io.sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (status < 0) {
        error_printf("Connection failed %d\n", status);
        __close_player(player);
        return -2;
    }
    player->io.fp = fdopen(player->io.sockfd, "r");
    if (NULL == player->io.fp) {
        __close_player(player);
        return -3;
    }

    if ((player->cmd_io.sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        error_printf("Socket creation error\n");
        return -104;
    }
    
    status =  connect(player->cmd_io.sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (status < 0) {
        error_printf("Connection failed %d\n", status);
        __close_player(player);
        return -102;
    }
    player->cmd_io.fp = fdopen(player->cmd_io.sockfd, "r");
    if (NULL == player->cmd_io.fp) {
        __close_player(player);
        return -103;
    }
    
    char *p = lms_query(player, "player count");
    int player_count = -1;
    player_count = atoi(p);
    FREE(p);
    if (player_count < 1) {
        error_printf("player count = %d\n", player_count);
        __close_player(player);
        return -6;
    }
   
    for(int ix=0; ix < player_count && player->lms_player_id == NULL; ++ix) {
         p = lms_query(player, "player ip %d", ix);
         char *c = p;
         while(*c != 0) {
             if (*c == '%') {
                 *c = 0;
                 break;
             }
             ++c;
         }
         local_addr_ptr lp = player->lp;
         while(lp) {
            if (0 == strcmp(lp->addr, p)) {
                FREE(p);
                p = lms_query(player, "player id %d", ix);
                player->lms_player_id = strdup(p);
            }
            lp = lp->next;
         }
        FREE(p);
    }
    if (player->lms_player_id == NULL) {
        error_printf("failed to establish player id\n");
        return -7;
    }
    return 0;
}

static void close_player(player_ptr player) {
    if (player) {
        __close_player(player);
        FREE(player->lms);
        ZERO(player);
    }
}

static int open_player(player_ptr player, const char* name) {
    int rv = -1;
    if (name) {
        ZERO(player);
        player->lms = strdup(name);
        rv = __open_player(player); 
        if (rv) {
            FREE(player->lms);
        }
    }
    return rv;
}

// public interface
void player_command(player_ptr player, const char* command) {
    if (player) {
        lms_command(player, command);
    }
}

player_ptr open_local_player(const char *lms_addr) {
    player_ptr player = calloc(1, sizeof(*player));
    if (player) {
        int status = open_player(player, lms_addr);
        if (status) {
            close_player(player);
            free(player);
            error_printf("open player to %s failed %d\n", lms_addr, status);
            return NULL;
        }
        clear_player_status(&player->status);
    }
    return player;
}

int poll_player(player_ptr player, player_transient_state_ptr ptransient) {
    if (player) {
        get_player_volume(player);
        if (ptransient) {
            ptransient->elapsed_secs = player->status.elapsed_time;
            ptransient->volume = player->volume;
        }
        return get_player_status(player);
    }
    ptransient->elapsed_secs = 0;
    ptransient->volume = 0;
    return 0;
}

player_ptr close_local_player(player_ptr player) {
    if (player) {
        close_player(player);
        free(player);
        player = NULL;
    }
    return player;
}

static pfv_type _get_player_value(player_ptr player, player_value_ptr pfv, const char *key) {
    pfv_type return_value = PFV_NONE;
#define PFV_INTVALUE(nm, gt) \
    if (player->status.nm > gt) { \
        return_value = PFV_INT; \
        pfv->integer = player->status.nm; \
    }

#define PFV_STRVALUE(nm) \
    if (player->status.nm) { \
        return_value = PFV_STRINGPTR; \
        pfv->strptr = player->status.nm; \
    }
    
    if (player == NULL) {
        return return_value;
    }

    player_key_hashv keyhashv = compute_player_hash(key);
    switch(keyhashv) {
        case NULL_KEY:
            break;
        case elapsed_time:
            PFV_INTVALUE(elapsed_time, -1);
            break;
        case remote:
            PFV_INTVALUE(remote, -1);
            break;
        case compilation:
            PFV_INTVALUE(compilation, -1);
            break;
        case can_seek:
            PFV_INTVALUE(can_seek, -1);
            break;
        case duration:
            PFV_INTVALUE(duration, -1);
            break;
        case playlist_cur_index:
            PFV_INTVALUE(playlist_cur_index, -1);
            break;
        case playlist_tracks:
            PFV_INTVALUE(playlist_tracks, -1);
            break;
        case randomplay:
            PFV_INTVALUE(randomplay, -1);
            break;
        case playlist_index:
            PFV_INTVALUE(playlist_index, -1);
            break;
        case year:
            PFV_INTVALUE(year, -1);
            break;
        case filesize:
            PFV_INTVALUE(filesize, -1);
            break;
        case disc:
            PFV_INTVALUE(disc, -1);
            break;
        case samplesize:
            PFV_INTVALUE(samplesize, -1);
            break;
        case lossless:
            PFV_INTVALUE(lossless, -1);
            break;
        case disccount:
            PFV_INTVALUE(disccount, -1);
            break;
        case tracknum:
            PFV_INTVALUE(tracknum, -1);
            break;
        case samplerate:
            PFV_INTVALUE(samplerate, -1);
            break;
        case playlist_repeat:
            PFV_INTVALUE(playlist_repeat, -1);
            break;
        case playlist_shuffle:
            PFV_INTVALUE(playlist_shuffle, -1);
            break;
        case playlist_duration:
            PFV_INTVALUE(playlist_duration, -1);
            break;
        case id:
            PFV_INTVALUE(id, -1);
            break;
        case waitingToPlay:
            PFV_INTVALUE(waitingToPlay, -1);
            break;
        case isClassical:
            PFV_INTVALUE(isClassical, -1);
            break;

        case current_title:
            PFV_STRVALUE(current_title);
            break;
        case title:
            PFV_STRVALUE(title);
            break;
        case album:
            PFV_STRVALUE(album);
            break;
        case type:
            PFV_STRVALUE(type);
            break;
        case bitrate:
            PFV_STRVALUE(bitrate);
            break;
        case composer:
            PFV_STRVALUE(composer);
            break;
        case conductor:
            PFV_STRVALUE(conductor);
            break;
        case band:
            PFV_STRVALUE(band);
            break;
        case artist:
            PFV_STRVALUE(artist);
            break;
        case albumartist:
            PFV_STRVALUE(albumartist);
            break;
        case trackartist:
            PFV_STRVALUE(trackartist);
            break;
        case release_type:
            PFV_STRVALUE(release_type);
            break;
        case genre:
            PFV_STRVALUE(genre);
            break;
        case genres:
            PFV_STRVALUE(genres);
            break;
        case remote_title:
            PFV_STRVALUE(remote_title);
            break;
        case album_replay_gain:
            PFV_STRVALUE(album_replay_gain);
            break;
        case replay_gain:
            PFV_STRVALUE(replay_gain);
            break;
        case subtitle:
            PFV_STRVALUE(subtitle);
            break;
        case mode:
            PFV_STRVALUE(mode);
            break;

        case player_name:
        case player_connected:
        case player_ip:
        case power:
        case signalstrength:
        case rate:
        case sync_master:
        case sync_slaves:
        case mixer_volume:
        case playlist_mode:
        case playlist_timestamp:
        case seq_no:
        case digital_volume_control:
        case use_volume_control:
        case remoteMeta:
        case bpm:
        case playlist_name:
        case playlist_modified:
        case playlist_id:
            puts("????????????");
            break;
        
        // Meta keys            
        case VOLUME:
            if (player->volume > -1) {
                return_value = PFV_INT;
                pfv->integer = player->volume;
            }break;
        case ARTIST:
            if (player->status.meta_artist) {
                PFV_STRVALUE(meta_artist);
            } else if (player->status.artist) {
                PFV_STRVALUE(artist);
            } else if (player->status.trackartist) {
                PFV_STRVALUE(trackartist);
            } else if (player->status.albumartist) {
                PFV_STRVALUE(albumartist);
            }
            break;
        case TITLE:
            if (player->status.title) {
                PFV_STRVALUE(title);
            } else if (player->status.current_title) {
                PFV_STRVALUE(current_title);
            } else if (player->status.subtitle) {
                PFV_STRVALUE(subtitle);
            }
            break;
        case PLAYLIST_CURRENT:
            if(player->status.playlist_cur_index > -1 && player->status.playlist_tracks > 1) { 
                return_value = PFV_INT;
                pfv->integer = player->status.playlist_cur_index+1;
            }
            break;
        case REMOTE_CHAR:
            if (player->status.remote > 0) {
                return_value = PFV_STRINGPTR;
                if (player->status.remote) {
                    pfv->strptr = "R";
                } else {
                    pfv->strptr = "L";
                }
            }break;
        case REMOTE:
            if (player->status.remote > 0) {
                return_value = PFV_STRINGPTR;
                if (player->status.remote) {
                    pfv->strptr =  "remote";
                } else {
                    pfv->strptr =  "";
                }
            }break;
        case COMPILATION_CHAR:
            if (player->status.compilation > 0) {
                return_value = PFV_STRINGPTR;
                if (player->status.compilation) {
                    pfv->strptr = "C";
                } else {
                    pfv->strptr = " ";
                }
            }break;
        case COMPILATION:
            if (player->status.compilation > 0) {
                if (player->status.compilation) {
                    return_value = PFV_STRINGPTR;
                    pfv->strptr =  "compilation";
                }
            }break;
        case GENRE:
            if (player->status.genre) {
                PFV_STRVALUE(genre);
            } else if(player->status.genres) {
                PFV_STRVALUE(genres);
            }
            break;
        case GENRES:
            if (player->status.genres) {
                PFV_STRVALUE(genres);
            } else if(player->status.genre) {
                PFV_STRVALUE(genre);
            }
            break;
        case LOSSLESS:
            if (player->status.lossless > 0) {
                if (player->status.lossless) {
                    return_value = PFV_STRINGPTR;
                    pfv->strptr =  "lossless";
                }
            }break;
        case REPEAT:
            switch(player->status.playlist_repeat) {
                default:
                    break;
                case 1:
                    return_value = PFV_STRINGPTR;
                    pfv->strptr =  "song";
                    break;
                case 2:
                    return_value = PFV_STRINGPTR;
                    pfv->strptr =  "playlist";
                    break;
            }
            break;
        case SHUFFLE:
            switch(player->status.playlist_shuffle) {
                default:
                    break;
                case 1:
                    return_value = PFV_STRINGPTR;
                    pfv->strptr =  "songs";
                    break;
                case 2:
                    return_value = PFV_STRINGPTR;
                    pfv->strptr =  "albums";
                    break;
            }
            break;
        case WAITING:
            if (player->status.waitingToPlay > 0) {
                return_value = PFV_STRINGPTR;
                pfv->strptr =  "waiting";
            }
            break;
         case RANDOMPLAY:
            if (player->status.waitingToPlay > 0) {
                return_value = PFV_INT;
                pfv->integer =  player->status.waitingToPlay;
            }
         case MODE:
            if (player->status.mode) {
                return_value = PFV_INT;
                if (0 == strcmp("stop", player->status.mode)) {
                    pfv->integer = 2;
                } else if (0 == strcmp("play", player->status.mode)) {
                    pfv->integer = 1;
                } else if (0 == strcmp("pause", player->status.mode)) {
                    pfv->integer = 0;
                }
            } break;
        case CAN_FWD:
            return_value = PFV_INT;
            pfv->integer = 0;
            if (player->status.playlist_tracks > 1) {
                pfv->integer = 1;
            }
            break;
        case CAN_REW:
            return_value = PFV_INT;
            pfv->integer = 0;
            if (player->status.playlist_tracks > 1 || player->status.can_seek) {
                pfv->integer = 1;
            }
            break;
        case DURATION:
            return_value = PFV_INT;
            pfv->integer = 0;
            if (player->status.duration > 0) {
                pfv->integer = player->status.duration;
            }
            break;
        case CAN_SEEK:
            return_value = PFV_INT;
            pfv->integer = player->status.can_seek < 1 ? 0: 1;
            break;
        case YEAR:
            if (player->status.year > 0) {
                return_value = PFV_INT;
                pfv->integer =  player->status.year;
            }
            break;
        case ALBUM_OR_REMOTE_TITLE:
            if (player->status.album) {
                return_value = PFV_STRINGPTR;
                pfv->strptr = player->status.album;
            } else if (player->status.remote_title) {
                return_value = PFV_STRINGPTR;
                pfv->strptr = player->status.remote_title;
            }
            break;
        case BITRATE:
            if (player->status.bitrate) {
                if (strcmp("0", player->status.bitrate)) {
                    return_value = PFV_STRINGPTR;
                    pfv->strptr = player->status.bitrate;
                }
            }
            break;
        case PLAYLIST_TRACKS:
            PFV_INTVALUE(playlist_tracks, 1);
            break;
    }
#undef PFV_INTVALUE
#undef PFV_STRVALUE
    return return_value;
}

pfv_type get_player_value(player_ptr player, player_value_ptr pfv, const char *key) {
    pfv_type pft = _get_player_value(player, pfv, key);
    switch(pft) {
        case PFV_NONE:
        case PFV_INT:
            break;
        case PFV_STRINGPTR:
            pfv->strptr = strdup(pfv->strptr);
            break;
    }
    return pft;
}

static int snprintf_time(char *buff, size_t bufflen, int seconds, bool suppress0) {
    int wr = 0;
    if (suppress0 && seconds < 1) {
        return 0;
    }
    int mins = seconds/60;
    int hours = mins/60;
    mins = mins%60;
    int secs = abs(seconds%60);
    if (hours) {
        mins = abs(mins);
        wr = snprintf(buff, bufflen, "%d:%02d:%02d", hours, mins, secs);
    } else {
        wr = snprintf(buff, bufflen, "%d:%02d", mins, secs);
    }
    return wr;
}

void player_sprintf(player_ptr player, char* buff, size_t bufflen, const char *format) {
    char* pre;
    char* post;
    char* fmt = strdup(format);
    char* scan = fmt;
    char* pprint = buff;
    int   wr;
    int   avail = bufflen;

    *pprint = '\0';
    if (player == NULL) {
        return;
    }

#define SNPRINTF(str) \
    wr = snprintf(pprint, avail, "%s", (str)); \
    avail -= wr; \
    if (avail < 1) goto END; \
    pprint += wr

#define SNPRINTF_INT_FIELD(integer) \
    if (aw) { \
        char fieldfmt[16]; \
        snprintf(fieldfmt, sizeof(fieldfmt), "%%%sd", aw); \
        wr = snprintf(pprint, avail, fieldfmt, (integer)); \
    } else { \
        wr = snprintf(pprint, avail, "%d", (integer)); \
    } \
    avail -= wr; \
    if (avail < 1) goto END; \
    pprint += wr

#define SNPRINTF_STR_FIELD(str) \
    if (aw) { \
        char fieldfmt[16]; \
        snprintf(fieldfmt, sizeof(fieldfmt), "%%%ss", aw); \
        wr = snprintf(pprint, avail, fieldfmt, (str)); \
    } else { \
        wr = snprintf(pprint, avail, "%s", (str)); \
    } \
    avail -= wr; \
    if (avail < 1) goto END; \
    pprint += wr


#define SNPRINTF_TIME(integer, suppress0) \
    wr = snprintf_time(pprint, avail, (integer), suppress0); \
    avail -= wr; \
    if (avail < 1) goto END; \
    pprint += wr


    while(*scan) {
        pre = scan;
        while(*scan && *scan != '[' && *scan != '{') {
            ++scan;
        }
        if (*scan == 0) goto END;
        if (*scan == '[') {
            *scan = '\0';
            SNPRINTF(pre);
            ++scan;
            pre = scan;
            while(*scan && *scan != '{') {
                ++scan;
            }
            if (*scan == 0) goto END;
            *scan='\0';
            ++scan;
            char *tok = scan;
            char *aw = NULL;
            while(*scan && *scan != '}') {
                if (*scan == ':') {
                    aw = scan + 1;
                    *scan = '\0';
                }
                ++scan;
            }
            if (*scan == 0) goto END;
            *scan = '\0';
            ++scan;
            post=scan;
            while(*scan && *scan != ']') {
                ++scan;
            }
            if (*scan == 0) goto END;
            *scan = '\0';
            player_value pfv;
            switch (_get_player_value(player, &pfv, tok)) {
                case PFV_NONE:
                    break;
                case PFV_INT:
                    SNPRINTF(pre);
                    switch(compute_player_hash(tok)){
                        default:
                            SNPRINTF_INT_FIELD(pfv.integer);
                            break;
                        case DURATION:
                            SNPRINTF_TIME(pfv.integer, true);
                            break;
                        case elapsed_time:
                            SNPRINTF_TIME(pfv.integer, false);
                            break;
                    }
                    SNPRINTF(post);
                    break;
                case PFV_STRINGPTR:
                    SNPRINTF(pre);
                    SNPRINTF_STR_FIELD(pfv.strptr);
                    SNPRINTF(post);
                    break;
            }
            pre = post = scan; // pre and post point to empty strings
            ++scan;
        } else if (*scan == '{') {
            post = scan;
            *scan = '\0';
            ++scan;
            char *tok = scan;
            char *aw = NULL;
            while(*scan && *scan != '}') {
                if (*scan == ':') {
                    aw = scan + 1;
                    *scan = '\0';
                }
                ++scan;
            }
            if (*scan == 0) goto END;
            *scan = '\0';
            player_value pfv;
            switch (_get_player_value(player, &pfv, tok)) {
                case PFV_NONE:
                    break;
                case PFV_INT:
                    SNPRINTF(pre);
                    switch(compute_player_hash(tok)){
                        default:
                            SNPRINTF_INT_FIELD(pfv.integer);
                            break;
                        case DURATION:
                            SNPRINTF_TIME(pfv.integer, true);
                            break;
                        case elapsed_time:
                            SNPRINTF_TIME(pfv.integer, false);
                            break;
                    }
                    break;
                case PFV_STRINGPTR:
                    SNPRINTF(pre);
                    SNPRINTF_STR_FIELD(pfv.strptr);
                    break;
            }
            pre = post = scan; // pre and post point to empty strings
            ++scan;
        }
    }
END:
    if (fmt) {
        free(fmt);
    }
}

void player_volume_set(player_ptr player, int level) {
    level = level < 0 ? 0 : level;
    level = level > 100 ? 100: level;
    lms_command(player, "mixer volume %d", level);
}

void player_volume_nudge(player_ptr player, int delta) {
    int volume = player->volume + delta;
    player_volume_set(player, volume);
}

void player_seek(player_ptr player, int seek_time) {
    if (player->status.can_seek) {
        seek_time = seek_time < 0 ? 0 : seek_time;
        if (player->status.duration > 0 && seek_time < player->status.duration) {
            lms_command(player, "time %d", seek_time);
        }
    }
}
