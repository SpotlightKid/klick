/*
 * klick - an advanced metronome for jack
 *
 * Copyright (C) 2007-2013  Dominic Sacré  <dominic.sacre@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "tempomap.hh"

#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <algorithm>
#include <boost/tokenizer.hpp>
#include <boost/make_shared.hpp>
#include <boost/bind.hpp>

#include "util/string.hh"
#include "util/regex.hh"
#include "util/lexical_cast.hh"

typedef boost::char_separator<char> char_sep;
typedef boost::tokenizer<char_sep> tokenizer;


#define LABEL   "([[:alnum:]_-]+)"
#define INT     "([[:digit:]]+)"
#define FLOAT   "([[:digit:]]+(\\.[[:digit:]]*)?|\\.[[:digit:]]+)"
#define PATTERN "([Xx.]+)"

// matches a line that contains nothing but whitespace or comments
static das::regex const REGEX_BLANK(
    "^[[:blank:]]*(#.*)?$"
);

// matches any valid line in a tempomap file
static das::regex const REGEX_VALID(
    // label
    "^[[:blank:]]*(" LABEL ":)?" \
    // bars
    "[[:blank:]]*" INT "" \
    // meter
    "([[:blank:]]+" INT "/" INT ")?" \
    // tempo
    "[[:blank:]]+" FLOAT "(-" FLOAT "|((," FLOAT ")*))?" \
    // pattern
    "([[:blank:]]+" PATTERN ")?" \
    // volume
    "([[:blank:]]+" FLOAT ")?" \
    // comment
    "[[:blank:]]*(#.*)?$"
);

static int const IDX_LABEL   =  2,
                 IDX_BARS    =  3,
                 IDX_BEATS   =  5,
                 IDX_DENOM   =  6,
                 IDX_TEMPO   =  7,
                 IDX_TEMPO2  = 10,
                 IDX_TEMPI   = 12,
                 IDX_PATTERN = 17,
                 IDX_VOLUME  = 19,
                 RE_NMATCHES = 22;


// matches valid tempo parameters on the command line
static das::regex const REGEX_CMDLINE(
    // bars
    "^[[:blank:]]*(" INT "[[:blank:]]+)?" \
    // meter
    "(" INT "/" INT "[[:blank:]]+)?" \
    // tempo
    FLOAT "(-" FLOAT "/" FLOAT ")?" \
    // pattern
    "([[:blank:]]+" PATTERN ")?[[:blank:]]*$"
);

static int const IDX_BARS_CMD    =  2,
                 IDX_BEATS_CMD   =  4,
                 IDX_DENOM_CMD   =  5,
                 IDX_TEMPO_CMD   =  6,
                 IDX_TEMPO2_CMD  =  9,
                 IDX_ACCEL_CMD   = 11,
                 IDX_PATTERN_CMD = 14,
                 RE_NMATCHES_CMD = 15;

#undef LABEL
#undef INT
#undef FLOAT
#undef PATTERN


TempoMap::Pattern TempoMap::parse_pattern(std::string const &s, int nbeats)
{
    Pattern pattern;

    if (!s.empty()) {
        if (static_cast<int>(s.length()) != nbeats) {
            throw ParseError("pattern length doesn't match number of beats");
        }
        pattern.resize(nbeats);
        for (int n = 0; n < nbeats; ++n) {
            pattern[n] = (s[n] == 'X') ? BEAT_EMPHASIS :
                         (s[n] == 'x') ? BEAT_NORMAL : BEAT_SILENT;
        }
    }
    return pattern;
}


std::string TempoMap::pattern_to_string(Pattern const & p)
{
    std::ostringstream os;

    for (Pattern::const_iterator i = p.begin(); i != p.end(); ++i) {
        os << (*i == BEAT_EMPHASIS ? "X" : *i == BEAT_NORMAL ? "x" : ".");
    }

    return os.str();
}


std::vector<float> TempoMap::parse_tempi(std::string const &s, float tempo1, int nbeats_total)
{
    std::vector<float> tempi;

    char_sep sep(",");
    tokenizer tok(s, sep);
    if (std::distance(tok.begin(), tok.end()) != nbeats_total - 1) {
        throw ParseError("number of tempo values doesn't match number of beats");
    }
    tempi.push_back(tempo1);
    for (tokenizer::iterator i = tok.begin(); i != tok.end(); ++i) {
        tempi.push_back(das::lexical_cast<float>(*i));
    }
    return tempi;
}


void TempoMap::check_entry(Entry const & e)
{
    if ((e.tempo <= 0 && e.tempi.empty()) ||
        std::find_if(e.tempi.begin(), e.tempi.end(), boost::bind(std::less_equal<float>(), _1, 0.0f)) != e.tempi.end()) {
        throw ParseError("tempo must be greater than zero");
    }
    if (e.bars <= 0 && e.bars != -1) {
        throw ParseError("number of bars must be greater than zero");
    }
    if (e.beats <= 0 || e.denom <= 0) {
        throw ParseError("invalid time signature");
    }
}


std::string TempoMap::dump() const
{
    std::ostringstream os;

    os << std::fixed << std::setprecision(2);

    for (Entries::const_iterator i = _entries.begin(); i != _entries.end(); ++i) {
        // label
        os << (i->label.length() ? i->label : "-") << ": ";
        // bars
        if (i->bars != -1) os << i->bars;
        else os << "*";
        os << " ";
        // meter
        os << i->beats << "/" << i->denom << " ";
        // tempo
        if (i->tempo) {
            os << i->tempo;
            if (i->tempo2) os << "-" << i->tempo2;
        } else {
            os << i->tempi[0];
            for (std::vector<float>::const_iterator k = i->tempi.begin() + 1; k != i->tempi.end(); ++k) {
                os << "," << *k;
            }
        }
        os << " ";
        // pattern
        if (i->pattern.empty()) {
            os << "-";
        } else {
            os << pattern_to_string(i->pattern);
        }
        os << " ";
        // volume
        os << i->volume;
        os << std::endl;
    }

    return os.str();
}


TempoMapPtr TempoMap::join(TempoMapConstPtr const m1, TempoMapConstPtr const m2)
{
    TempoMapPtr m(new TempoMap());

    std::back_insert_iterator<Entries> p(m->_entries);

    std::copy(m1->entries().begin(), m1->entries().end(), p);
    std::copy(m2->entries().begin(), m2->entries().end(), p);

    return m;
}


/*
 * loads tempomap from a file
 */
TempoMapPtr TempoMap::new_from_file(std::string const & filename)
{
    std::istream *input;
    boost::shared_ptr<std::ifstream> file;

    if (filename == "-") {
        input = &std::cin;
    }
    else {
        file = boost::make_shared<std::ifstream>(filename.c_str());

        if (!file->is_open()) {
            throw std::runtime_error(das::make_string() << "can't open tempo map file: '" << filename << "'");
        }

        input = file.get();
    }

    TempoMapPtr map(new TempoMap());

    std::string line;
    int lineno = 0;

    while (!input->eof()) {
        std::getline(*input, line);
        lineno++;

        // discard blank lines right away
        if (REGEX_BLANK.match(line)) {
            continue;
        }

        try {
            // check if this line matches the regex
            das::regex::matches matches = REGEX_VALID.match(line, RE_NMATCHES);
            if (!matches) {
                throw ParseError("malformed tempo map entry");
            }

            Entry e;

            e.label   = matches[IDX_LABEL];
            e.bars    = das::lexical_cast<int>(matches[IDX_BARS]);
            e.tempo   = das::lexical_cast<float>(matches[IDX_TEMPO]);
            e.tempo2  = das::lexical_cast<float>(matches[IDX_TEMPO2], 0);
            e.beats   = das::lexical_cast<int>(matches[IDX_BEATS], 4);
            e.denom   = das::lexical_cast<int>(matches[IDX_DENOM], 4);
            e.pattern = parse_pattern(matches[IDX_PATTERN], e.beats);
            e.volume  = das::lexical_cast<float>(matches[IDX_VOLUME], 1.0f);

            if (matches.has(IDX_TEMPI)) {
                e.tempi = parse_tempi(matches[IDX_TEMPI], e.tempo, e.beats * e.bars);
                e.tempo = 0.0f;
            }

            check_entry(e);
            map->_entries.push_back(e);
        }
        catch (ParseError const & e) {
            throw ParseError(das::make_string() << e.what() << ":\n"
                                << "line " << lineno << ": " << line);
        }
    }

    return map;
}


/*
 * loads single-line tempomap from a string
 */
TempoMapPtr TempoMap::new_from_cmdline(std::string const & line)
{
    TempoMapPtr map(new TempoMap());

    try {
        das::regex::matches matches = REGEX_CMDLINE.match(line, RE_NMATCHES_CMD);
        if (!matches) {
            throw ParseError("malformed tempo map string");
        }

        Entry e;

        e.label   = "";
        e.bars    = das::lexical_cast<int>(matches[IDX_BARS_CMD], -1);
        e.beats   = das::lexical_cast<int>(matches[IDX_BEATS_CMD], 4);
        e.denom   = das::lexical_cast<int>(matches[IDX_DENOM_CMD], 4);
        e.tempo   = das::lexical_cast<float>(matches[IDX_TEMPO_CMD]);
        e.tempo2  = 0.0f;
        e.pattern = parse_pattern(matches[IDX_PATTERN_CMD], e.beats);
        e.volume  = 1.0f;

        check_entry(e);

        if (!matches.has(IDX_TEMPO2_CMD)) {
            // no tempo change, just add this single entry
            map->_entries.push_back(e);
        } else {
            // tempo change...
            e.tempo2 = das::lexical_cast<float>(matches[IDX_TEMPO2_CMD]);
            float accel = das::lexical_cast<float>(matches[IDX_ACCEL_CMD]);
            if (accel <= 0.0f) {
                throw ParseError("accel must be greater than zero");
            }
            int bars_total = e.bars;
            int bars_accel = static_cast<int>(std::ceil(accel * std::abs(e.tempo2 - e.tempo)));

            if (bars_total == -1 || bars_total > bars_accel) {
                e.bars = bars_accel;
                map->_entries.push_back(e);

                // add a second entry, to be played once the "target" tempo is reached
                e.bars = bars_total == -1 ? -1 : bars_total - bars_accel;
                e.tempo = e.tempo2;
                e.tempo2 = 0.0f;
                map->_entries.push_back(e);
            } else {
                // total number of bars is shorter than acceleration
                e.bars = bars_total;
                e.tempo2 = e.tempo + (e.tempo2 - e.tempo) * bars_total / bars_accel;
                map->_entries.push_back(e);
            }
        }
    }
    catch (ParseError const & e) {
        throw ParseError(das::make_string() << e.what() << ":\n" << line);
    }

    return map;
}


/*
 * creates tempomap with one single entry
 */
TempoMapPtr TempoMap::new_simple(int bars, float tempo, int beats, int denom,
                                 Pattern const & pattern, float volume)
{
    TempoMapPtr map(new TempoMap());

    Entry e;
    e.bars    = bars;
    e.tempo   = tempo;
    e.tempo2  = 0.0f;
    e.beats   = beats;
    e.denom   = denom;
    e.volume  = volume;
    e.pattern = pattern;

    map->_entries.push_back(e);

    return map;
}
