#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

#include "lyrion_player.h"

const char* player_key_strings[] = {
    "player_name",
    "player_connected",
    "player_ip",
    "power",
    "signalstrength",
    "mode",
    "remote",
    "current_title",
    "time",
    "rate",
    "duration",
    "sync_master",
    "sync_slaves",
    "mixer_volume",
    "playlist repeat",              // field playlist_repeat
    "playlist shuffle",             // field playlist_shuffle
    "playlist duration",            // field playlist_duration
    "playlist mode",                // field playlist_off
    "seq_no",
    "playlist_cur_index",
    "playlist_timestamp",
    "playlist_tracks",
    "randomplay",
    "digital_volume_control",
    "use_volume_control",
    "remoteMeta",
    "playlist_index",
    "id",
    "title",
    "album",
    "type",
    "bitrate",
    "year",
    "composer",
    "conductor",
    "band",
    "albumartist",
    "trackartist",
    "release_type",
    "filesize",
    "genre",
    "genres",
    "disc",
    "samplesize",
    "bpm",
    "remote_title",
    "lossless",
    "disccount",
    "tracknum",
    "samplerate",
    "album_replay_gain",
    "replay_gain",
    "subtitle",
    "artist",
    "compilation",

    // meta key values
    "VOLUME",
    "ARTIST",
    "TITLE",
    "PLAYLIST_CURRENT",
    "REMOTE_CHAR",
    "REMOTE",
    "COMPILATION_CHAR",
    "COMPILATION",
    "GENRE",
    "GENRE_ALL",
    "GENRES",
    "LOSSLESS",
    "REPEAT",
    "SHUFFLE",
    "RANDOMPLAY",
    "MODE",
};

void ppoll(player_ptr player) {
    int v = (poll_player(player, NULL));
    if (v) {
        putchar('\n');
    } else {
        putchar('\r');
    }
    char buffer[1000];
    player_sprintf(player, buffer, sizeof(buffer), 
            "[vol={VOLUME}]"
            " {time:03}[/{duration}]"
            " ▒"
            " • [{disc}.]{tracknum:02} • {TITLE}"
            " ▒"
            "[ {ARTIST}][ • {album}][ ({year})]"
            " ▒"
            " [{PLAYLIST_CURRENT}/][{playlist_tracks} ][ • {GENRES}][ • {type}][ • {bitrate}][ • {samplerate} Hz][ • {samplesize} bits]"
            " █"
//            "[ • ({REMOTE_CHAR})][ • {LOSSLESS}][ • ({COMPILATION})][ • repeat:{REPEAT}][ • shuffle:{SHUFFLE}][ • pd:{playlist duration}]"
            "[ • ({REMOTE_CHAR})][ • {LOSSLESS}][ • ({COMPILATION})][ • repeat:{playlist repeat}][ • shuffle:{playlist shuffle}][ • pd:{playlist duration}]"
            "[ • CT:{current_title}][ • RT:{remote_title}] • {mode} {MODE}"
            " ▒"
            "[ • id:{id}][ • seekable:{can_seek}][ • randomplay:{RANDOMPLAY}][ • {WAITING}]" 
    );
    printf("%s", buffer);
    fflush(stdout);
}

int main(int argc, char const* argv[])
{
    if (argc > 2 && 0 == strcmp("poll", argv[1])) {
        player_ptr player = open_local_player(argv[2]);
        if (player == NULL) {
            printf("failed to find local player connected to LMS at %s\n", argv[2]);
        }


        ppoll(player);
    //    player_play(player);
    //    player_fwd(player);

        for(int n = 0; 
                argc >3 && n < (argv[3] ? atoi(argv[3]) - 1 : 0);
                ++n) {
            sleep(1);
            ppoll(player);
        }
        puts("");

    //    player_rew(player);
        sleep(1);
    //    player_pause(player);
        close_local_player(player);
    } else if (argc > 1 && 0 == strcmp("hashes", argv[1])) {
        for(int ix=0; ix < sizeof(player_key_strings)/sizeof(player_key_strings[0]); ++ix) {
            printf("    %s = 0x%09lx,\n", player_key_strings[ix], compute_player_hash(player_key_strings[ix]));
        }
    } else if (argc > 2 && 0 == strcmp("hash", argv[1])) {
        for (int ix = 2; ix < argc; ++ix) {
            printf("    %s = 0x%09lx,\n", argv[ix], compute_player_hash(argv[ix]));
        }
    } else if (argc > 3 && 0 == strcmp("command", argv[1])) {
        player_ptr player = open_local_player(argv[2]);
        if (player == NULL) {
            printf("failed to find local player connected to LMS at %s\n", argv[2]);
        }
        for (int ix = 3; ix < argc; ++ix) {
            uint64_t hashv = compute_player_hash(argv[ix]);
            switch(hashv){
                case  0x0000b628c:
                    player_play(player);
                    break;
                case  0x0000780be:
                    player_stop(player);
                    break;
                case  0x0004f6796:
                    player_pause(player);
                    break;
                case  0x000005704:
                    player_rew(player);
                    break;
                case  0x0000011d3:
                    player_fwd(player);
                    break;
                default:
                    printf("unknown command %s\n", argv[ix]);
                    break;
            }
        }
    } else {
        printf("usage:\n1) %s poll <lms> <N seconds>\n2) hashes\n3) hash <string>\n4)command <lms> <command>\n",
                argv[0]);
    }

    return 0;
}
