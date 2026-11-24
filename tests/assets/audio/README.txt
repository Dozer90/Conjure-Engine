Audio fixtures (code sweep F194): three 0.5 s, 440 Hz, mono, 48 kHz sine tones at amplitude 1/8 (FFmpeg's
sine source) the audio decoder tests read on every leg. They are self-made, so no download and no licence
question. Made once with FFmpeg 9.0.2 (essentials build, www.gyan.dev) from this folder, one command per line
(cmd, PowerShell or any POSIX shell):

    ffmpeg -f lavfi -i sine=frequency=440:duration=0.5:sample_rate=48000 -map_metadata -1 -fflags +bitexact -flags:a +bitexact -ac 1 -c:a libvorbis -q:a 2 tone.ogg
    ffmpeg -f lavfi -i sine=frequency=440:duration=0.5:sample_rate=48000 -map_metadata -1 -fflags +bitexact -flags:a +bitexact -ac 1 -c:a libmp3lame -b:a 64k tone.mp3
    ffmpeg -f lavfi -i sine=frequency=440:duration=0.5:sample_rate=48000 -map_metadata -1 -fflags +bitexact -flags:a +bitexact -ac 1 -c:a flac -sample_fmt s16 tone.flac
