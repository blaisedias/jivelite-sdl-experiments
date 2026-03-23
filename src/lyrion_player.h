#ifndef _lyrion_player_h_
#define _lyrion_player_h_
#include <stdlib.h>
#include <stdint.h>
#include "types.h"

typedef struct lyrion_player* player_ptr;

typedef struct {
    int elapsed_secs;
    int volume;
}player_transient_state,*player_transient_state_ptr;

int open_player(player_ptr player);
void close_player(player_ptr player);
player_ptr open_local_player(const char *lms_addr);
player_ptr close_local_player(player_ptr player);

uint64_t compute_player_hash(const char* s);

// returns 0 if non transient values have changed
int poll_player(player_ptr player, player_transient_state_ptr transient);

typedef enum {
    PFV_NONE,
    PFV_INT,
    PFV_STRINGPTR
} pfv_type;

typedef union {
    int  integer;
    const char* strptr;
}player_value, *player_value_ptr;

typedef enum {
    PLAYER_MODE_PAUSED,
    PLAYER_MODE_PLAYING,
    PLAYER_MODE_STOPPED,
    PLAYER_MODE_UNDEFINED
} player_mode_t;

/*
    for key values see LMS CLI status documentation:
        integer value keys:
            time, duration  ( time values in seconds )
            remote, compilation, lossless, randomplay ( flags: 0 or 1)
            playlist_cur_index, playlist_tracks, playlist_index, year,
            filesize, disc, samplesize,  disccount, tracknum, samplerate,
            playlist shuffle, playlist repeat

        string value keys:
            current_title, title, album, type, bitrate, composer, conductor,
            band, artist, albumartist, trackartist, release_type, genre, genres,
            remote_title, album_replay_gain, replay_gain, subtitle,
*/
pfv_type get_player_value(player_ptr player, player_value_ptr pfv, const char *key);

/*
   compose a string reflecting player state
   format
   field: {<player_key>:[align][fill][width]}
    if the player_key does not exist no text is emitted
        align: is C printf align
        fill: is C printf fill
        width: is C printf width
    Note:
        width is advisory in the sense that it will not truncate rendered text
        fill is invalid for string values
   annotated field:
   [.*<field>.*]
    if the player_key does not exist, none of the text within [] is emitted
    special characters: {, }, [, ], % (do not use)
  
    for key values see LMS CLI status documentation:
        integer value keys:
            time, duration  ( time values in seconds )
            remote, compilation, lossless, randomplay ( flags: 0 or 1)
            playlist_cur_index, playlist_tracks, playlist_index, year,
            filesize, disc, samplesize,  disccount, tracknum, samplerate,
            playlist shuffle, playlist repeat

        string value keys:
            current_title, title, album, type, bitrate, composer, conductor,
            band, artist, albumartist, trackartist, release_type, genre, genres,
            remote_title, album_replay_gain, replay_gain, subtitle,

        string meta value keys: (these keys do not exist in LMS CLI status
            VOLUME: player volume
            ARTIST: one of artist, trackartist, albumartist in that order
            TITLE:  one of title, remote_title, current_title, subtitle in that order
            PLAYLIST_CURRENT: playlist_cur_index+1
            REMOTE_CHAR: remote flag rendered as "L" or "R" 
            REMOTE: remote flag rendered as "" or "remote"
            COMPILATION_CHAR: compilation flag rendered as " " or "C"
            COMPILATION: compilation flag rendered as "" or "compilation"
            GENRE: one of genre or genres in that order
            GENRES: one of genres or genre in that order
            LOSSLESS: lossless flag rendered as "" or "lossless"
            REPEAT: one of "", "song", "playlist"
            SHUFFLE: one of "songs", "albums"
            WAITING: one of "", "waiting"
            RANDOMPLAY: integer value > 0 (TBD)
 */
void player_sprintf(player_ptr player, char* buff, size_t bufflen, const char *format);

// lyrion media server player control
void player_command(player_ptr player, const char* command);
#define player_play(player) player_command(player, "button play.single")
#define player_stop(player) player_command(player, "button pause.hold")
#define player_pause(player) player_command(player, "button pause.single")
#define player_fwd(player) player_command(player, "button fwd.single")
#define player_rew(player) player_command(player, "button rew.single")

#define player_shuffle_toggle(player) player_command(player, "button shuffle.single")
#define player_random_play(player) player_command(player, "button shuffle.hold")
#define player_shuffle_off(player) player_command(player, "button shuffle_off")
#define player_shuffle_on(player) player_command(player, "button shuffle_on")

#define player_repeat_toggle(player) player_command(player, "button repeat")
#define player_repeat_off(player) player_command(player, "button repeat_off")
#define player_repeat_one(player) player_command(player, "button repeat_one")
#define player_repeat_all(player) player_command(player, "button repeat_all")

#define player_power_toggle(player) player_command(player, "button power")
#define player_power_on(player) player_command(player, "button power_on")
#define player_power_off(player) player_command(player, "button power_off")

void player_volume_set(player_ptr player, int delta);
void player_volume_nudge(player_ptr player, int delta);
#define player_volume_inc(player) player_volume_change(player, step)
#define player_volume_dec(player) player_volume_change(player, step*-1)
void player_seek(player_ptr player, int time);

#endif // _lyrion_player_h_
