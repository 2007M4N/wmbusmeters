/*
 Copyright (C) 2017-2019 Fredrik Öhrström

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include"cmdline.h"
#include"meters.h"
#include"util.h"

#include<string>

using namespace std;

unique_ptr<CommandLine> parseCommandLine(int argc, char **argv) {

    CommandLine * c = new CommandLine;

    int i=1;
    const char *filename = strrchr(argv[0], '/')+1;
    if (!strcmp(filename, "wmbusmetersd")) {
        c->daemon = true;
        if (argc > 1) {
            error("Usage error: wmbusmetersd does not accept any arguments.\n");
        }
        return unique_ptr<CommandLine>(c);
    }
    if (argc < 2) {
        c->need_help = true;
        return unique_ptr<CommandLine>(c);
    }
    while (argv[i] && argv[i][0] == '-') {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-help") || !strcmp(argv[i], "--help")) {
            c->need_help = true;
            return unique_ptr<CommandLine>(c);
        }
        if (!strcmp(argv[i], "--silence")) {
            c->silence = true;
            i++;
            continue;
        }
        if (!strcmp(argv[i], "--verbose")) {
            c->verbose = true;
            i++;
            continue;
        }
        if (!strcmp(argv[i], "--debug")) {
            c->debug = true;
            i++;
            continue;
        }
        if (!strcmp(argv[i], "--c1")) {
            if (c->link_mode_set) {
                error("You have already specified a link mode!\n");
            }
            c->link_mode = LinkModeC1;
            c->link_mode_set = true;
            i++;
            continue;
        }
        if (!strcmp(argv[i], "--t1")) {
            if (c->link_mode_set) {
                error("You have already specified a link mode!\n");
            }
            c->link_mode = LinkModeT1;
            c->link_mode_set = true;
            i++;
            continue;
        }
        if (!strcmp(argv[i], "--logtelegrams")) {
            c->logtelegrams = true;
            i++;
            continue;
        }
        if (!strcmp(argv[i], "--useconfig")) {
            c->useconfig = true;
            if (i > 1 || argc > 2) {
                error("Usage error: --useconfig implies no other arguments on the command line.\n");
            }
            return unique_ptr<CommandLine>(c);
        }
        if (!strcmp(argv[i], "--reload")) {
            c->reload = true;
            if (i > 1 || argc > 2) {
                error("Usage error: --reload implies no other arguments on the command line.\n");
            }
            return unique_ptr<CommandLine>(c);
        }
        if (!strncmp(argv[i], "--robot", 7)) {
            if (strlen(argv[i]) == 7 ||
                (strlen(argv[i]) == 12 &&
                 !strncmp(argv[i]+7, "=json", 5)))
            {
                c->json = true;
                c->fields = false;
            }
            else if (strlen(argv[i]) == 14 &&
                     !strncmp(argv[i]+7, "=fields", 7))
            {
                c->json = false;
                c->fields = true;
                c->separator = ';';
            } else {
                error("Unknown output format: \"%s\"\n", argv[i]+7);
            }
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--separator=", 12)) {
            if (!c->fields) {
                error("You must specify --robot=fields before --separator=X\n");
            }
            if (strlen(argv[i]) != 13) {
                error("You must supply a single character as the field separator.\n");
            }
            c->separator = argv[i][12];
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--meterfiles", 12)) {
            c->meterfiles = true;
            if (strlen(argv[i]) > 12 && argv[i][12] == '=') {
                size_t len = strlen(argv[i])-13;
                if (len > 0) {
                    c->meterfiles_dir = string(argv[i]+13, len);
                } else {
                    c->meterfiles_dir = "/tmp";
                }
            } else {
                c->meterfiles_dir = "/tmp";
            }
            if (!checkIfDirExists(c->meterfiles_dir.c_str())) {
                error("Cannot write meter files into dir \"%s\"\n", c->meterfiles_dir.c_str());
            }
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--shell=", 8)) {
            string cmd = string(argv[i]+8);
            if (cmd == "") {
                error("The shell command cannot be empty.\n");
            }
            c->shells.push_back(cmd);
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--shellenvs", 11)) {
            c->list_shell_envs = true;
            i++;
            continue;
        }
        if (!strcmp(argv[i], "--oneshot")) {
            c->oneshot = true;
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--exitafter=", 12) && strlen(argv[i]) > 12) {
            c->exitafter = parseTime(argv[i]+12);
            if (c->exitafter <= 0) {
                error("Not a valid time to exit after. \"%s\"\n", argv[i]+12);
            }
            i++;
            continue;
        }
        if (!strcmp(argv[i], "--")) {
            i++;
            break;
        }
        error("Unknown option \"%s\"\n", argv[i]);
    }

    c->usb_device = argv[i];
    i++;
    if (c->usb_device.length() == 0) error("You must supply the usb device to which the wmbus dongle is connected.\n");

    if ((argc-i) % 4 != 0) {
        error("For each meter you must supply a: name,type,id and key.\n");
    }
    int num_meters = (argc-i)/4;

    for (int m=0; m<num_meters; ++m) {
        string name = argv[m*4+i+0];
        string type = argv[m*4+i+1];
        string id = argv[m*4+i+2];
        string key = argv[m*4+i+3];

        MeterType mt = toMeterType(type);

        if (mt == UNKNOWN_METER) error("Not a valid meter type \"%s\"\n", type.c_str());
        if (!isValidId(id)) error("Not a valid meter id \"%s\"\n", id.c_str());
        if (!isValidKey(key)) error("Not a valid meter key \"%s\"\n", key.c_str());
        c->meters.push_back(MeterInfo(name,type,id,key));
    }

    return unique_ptr<CommandLine>(c);
}
