/** @file xapian-compact.cc
 * @brief Compact a database, or merge and compact several.
 */
/* Copyright (C) 2003,2004,2005,2006,2007,2008,2009,2010 Olly Betts
 * Copyright (C) 2008 Lemur Consulting Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#include <config.h>

#include <xapian.h>

#include <cstdlib>
#include <iostream>

#include "gnu_getopt.h"

using namespace std;

#define PROG_NAME "xapian-compact"
#define PROG_DESC "Compact a database, or merge and compact several"

#define OPT_HELP 1
#define OPT_VERSION 2
#define OPT_NO_RENUMBER 3

static void show_usage() {
    cout << "Usage: "PROG_NAME" [OPTIONS] SOURCE_DATABASE... DESTINATION_DATABASE\n\n"
"Options:\n"
"  -b, --blocksize   Set the blocksize in bytes (e.g. 4096) or K (e.g. 4K)\n"
"                    (must be between 2K and 64K and a power of 2, default 8K)\n"
"  -n, --no-full     Disable full compaction\n"
"  -F, --fuller      Enable fuller compaction (not recommended if you plan to\n"
"                    update the compacted database)\n"
"  -m, --multipass   If merging more than 3 databases, merge the postlists in\n"
"                    multiple passes (which is generally faster but requires\n"
"                    more disk space for temporary files)\n"
"      --no-renumber Preserve the numbering of document ids (useful if you have\n"
"                    external references to them, or have set them to match\n"
"                    unique ids from an external source).  Currently this\n"
"                    option is only supported when merging databases if they\n"
"                    have disjoint ranges of used document ids\n"
"  --help            display this help and exit\n"
"  --version         output version information and exit" << endl;
}

class MyCompactor : public Xapian::Compactor {
  public:
    void set_status(const string & table, const string & status);

    string
    resolve_duplicate_metadata(const string & key,
			       const string & tag1,
			       const string & tag2);
};

void
MyCompactor::set_status(const string & table, const string & status)
{
    if (!status.empty())
	cout << '\r' << table << ": " << status << endl;
    else
	cout << table << " ..." << flush;
}

string
MyCompactor::resolve_duplicate_metadata(const string & key,
					const string & tag1,
					const string & tag2)
{
    (void)key;
    if (tag1 != tag2)
	cerr << "Warning: duplicate user metadata key with different tag value - picking arbitrary tag value" << endl;
    return tag1;
}

int
main(int argc, char **argv)
{
    const char * opts = "b:nFm";
    const struct option long_opts[] = {
	{"fuller",	no_argument, 0, 'F'},
	{"no-full",	no_argument, 0, 'n'},
	{"multipass",	no_argument, 0, 'm'},
	{"blocksize",	required_argument, 0, 'b'},
	{"no-renumber", no_argument, 0, OPT_NO_RENUMBER},
	{"help",	no_argument, 0, OPT_HELP},
	{"version",	no_argument, 0, OPT_VERSION},
	{NULL,		0, 0, 0}
    };

    MyCompactor compactor;

    int c;
    while ((c = gnu_getopt_long(argc, argv, opts, long_opts, 0)) != -1) {
	switch (c) {
	    case 'b': {
		char *p;
		size_t block_size = strtoul(optarg, &p, 10);
		if (block_size <= 64 && (*p == 'K' || *p == 'k')) {
		    ++p;
		    block_size *= 1024;
		}
		if (*p || block_size < 2048 || block_size > 65536 ||
		    (block_size & (block_size - 1)) != 0) {
		    cerr << PROG_NAME": Bad value '" << optarg
			 << "' passed for blocksize, must be a power of 2 between 2K and 64K"
			 << endl;
		    exit(1);
		}
		compactor.set_block_size(block_size);
		break;
	    }
	    case 'n':
		compactor.set_compaction_level(compactor.STANDARD);
		break;
	    case 'F':
		compactor.set_compaction_level(compactor.FULLER);
		break;
	    case 'm':
		compactor.set_multipass(true);
		break;
	    case OPT_NO_RENUMBER:
		compactor.set_renumber(false);
		break;
	    case OPT_HELP:
		cout << PROG_NAME" - "PROG_DESC"\n\n";
		show_usage();
		exit(0);
	    case OPT_VERSION:
		cout << PROG_NAME" - "PACKAGE_STRING << endl;
		exit(0);
	    default:
		show_usage();
		exit(1);
	}
    }

    if (argc - optind < 2) {
	show_usage();
	exit(1);
    }

    // Path to the database to create.
    compactor.set_destdir(argv[argc - 1]);

    try {
	for (int i = optind; i < argc - 1; ++i) {
	    compactor.add_source(argv[i]);
	}

	compactor.compact();
    } catch (const Xapian::Error &error) {
	cerr << argv[0] << ": " << error.get_description() << endl;
	exit(1);
    } catch (const char * msg) {
	cerr << argv[0] << ": " << msg << endl;
	exit(1);
    }
}