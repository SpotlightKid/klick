/*
 * klick - an advanced metronome for jack
 *
 * Copyright (C) 2007-2009  Dominic Sacré  <dominic.sacre@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "metronome.hh"
#include "audio_interface_jack.hh"

#include <boost/bind.hpp>

#include "util/debug.hh"


Metronome::Metronome(AudioInterface & audio)
  : _audio(audio)
  , _active(false)
{
}


void Metronome::register_process_callback()
{
    _audio.set_process_callback(boost::bind(&Metronome::process_callback, this, _1, _2));
}


void Metronome::register_timebase_callback()
{
    AudioInterfaceTransport *a = dynamic_cast<AudioInterfaceTransport*>(&_audio);
    if (a) {
        a->set_timebase_callback(boost::bind(&Metronome::timebase_callback, this, _1));
    }
}


void Metronome::set_active(bool b)
{
    if (b) {
        do_start();
    } else {
        do_stop();
    }

    _active = b;
}


void Metronome::set_sound(AudioChunkConstPtr emphasis, AudioChunkConstPtr normal)
{
    _click_emphasis = emphasis;
    _click_normal = normal;
}


void Metronome::play_click(bool emphasis, nframes_t offset, float volume)
{
    ASSERT(_click_emphasis);
    ASSERT(_click_normal);

    AudioChunkConstPtr click = emphasis ? _click_emphasis : _click_normal;

    _audio.play(click, offset, volume);
}


/*
MetronomeTransport::MetronomeTransport(AudioInterfaceTransport & audio)
  : Metronome(audio)
  , _audio_transport(audio)
{
}


void MetronomeTransport::register_timebase_callback()
{
    AudioInterfaceTransport *a = dynamic_cast<AudioInterfaceTransport*>(&_audio);
    if (a) {
        a->set_timebase_callback(boost::bind(&MetronomeTransport::timebase_callback, this, _1));
    }
}
*/
