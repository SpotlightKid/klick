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

#ifndef _AUDIO_INTERFACE_SNDFILE_HH
#define _AUDIO_INTERFACE_SNDFILE_HH

#include "audio_interface.hh"

#include <string>
#include <boost/shared_ptr.hpp>
#include <sndfile.h>


class AudioInterfaceSndfile
  : public AudioInterface
{
  public:

    AudioInterfaceSndfile(std::string const & filename, nframes_t samplerate);

    void process(std::size_t buffer_size);

    nframes_t samplerate() const { return _samplerate; }
    bool is_shutdown() const { return false; }

  private:

    std::string get_filename_extension(std::string const & filename);

    nframes_t _samplerate;
    boost::shared_ptr<SNDFILE> _sndfile;
};


#endif // _AUDIO_INTERFACE_SNDFILE_HH

