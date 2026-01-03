#!/usr/bin/env python3
r'''
    python script which provides an alternative for auto dependency
    for Makefiles without using .d files

    See:
    https://www.gnu.org/software/make/manual/make.html#Automatic-Prerequisites
    %.d: %.c
        @set -e; rm -f $@; \
         $(CC) -M $(CPPFLAGS) $< > $@.$$$$; \
         sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
         rm -f $@.$$$$

    include $(sources:.c=.d)
'''
import os
import os.path
import sys
import re

from optparse import OptionParser

re_c_file = re.compile(r"^..*\.(c|cpp|cxx)$", re.I)
re_hdr_file = re.compile(r"^..*\.(h|hpp|hxx)$", re.I)
re_src_files = re.compile(r"^..*\.(c|cpp|cxx|h|hpp|hxx)$", re.I)
re_include = re.compile(r'^#include\s*"(?P<incf>.*)"$')


def scan(dpath, deps):
    '''
    Scan source files in path to create dependency table
    '''
    for (path, _, files) in os.walk(dpath):
        # temp fix scan depth to 1
        if path != dpath:
            continue
        files.sort()
        for f in files:
            if re_src_files.match(f):
                srcpath = f"{path}/{f}"
                deps[srcpath] = []
                with open(srcpath, encoding='utf-8') as fp:
                    try:
                        lines = fp.readlines()
                    except UnicodeDecodeError:
                        lines = []
                for line in lines:
                    line = line.strip()
                    m = re_include.match(line)
                    if m:
                        try:
                            deps[srcpath].append(m.groupdict()['incf'])
                        except KeyError:
                            deps[srcpath] = [m.groupdict()['incf']]


def fix_inc_dep(inc, deps):
    '''
    fix include dependencies
    '''
    #  FIXME: will not handle multiplicate file names in different trees correctly.
    if inc in deps.keys():
        return inc
    rv = []

    for path in deps.keys():
        pcomps = path.split('/')
        icomps = inc.split('/')
        while (len(pcomps) > len(icomps)):
            pcomps.pop(0)
        if pcomps == icomps:
            rv.append(path)

    if len(rv) == 1:
        return rv[0]

    if len(rv) > 1:
        raise RuntimeError(
            "FIXME: Too dumb! multiple files matching include spec")

    print("----- ", inc)
    raise RuntimeError(f'No file found matching {inc}')


def gen_deps(output, regex, deps, objdir='', globaldeps=''):
    '''
        Generate the dependencies as an array of strings
    '''
    # for path, incs in sorted(deps.iteritems()):
    for path in sorted(deps.keys()):
        incs = deps[path]
        if len(incs) and regex.search(path):
            obj = f'{objdir}{path.split('/')[-1].split('.')[0]}.o'
            pdeps = list(incs)
            for inc in incs:
                pdeps.extend(deps[inc])
            output.append(f'{obj} : {path} {" ".join(sorted(pdeps))}'
                          f' {globaldeps}')
            output.append('')


def makedeps():
    '''
    main business logic of this script
    '''
    parser = OptionParser()
    parser.add_option("-v", "--verbose", dest="verbose", action="store_true",
                      default=False)
    parser.add_option("-o", "--objdir", dest="objdir", default="")
    parser.add_option("-f", "--file", dest="makefiledeps", action="store",
                      default="Makefile.deps")
    parser.add_option("-g", "--global", dest="globaldeps", action="store",
                      default='')
    (options, args) = parser.parse_args()
    if len(options.objdir) and options.objdir[-1] != '/':
        options.objdir = f'{options.objdir}/'

    deps = {}
    for arg in args:
        scan(arg, deps)

#    for _, includes in sorted(deps.iteritems()):
    for k in sorted(deps.keys()):
        deps[k] = [fix_inc_dep(include, deps) for include in deps[k]]
#        includes = deps[k]
#        for i in range(len(includes)):
#            includes[i] = fix_inc_dep(includes[i], deps)

    dependency_text_lines = []
    gen_deps(dependency_text_lines, re_c_file, deps,
             options.objdir, options.globaldeps)

    try:
        with open(options.makefiledeps, 'rb') as fp:
            current_dependency_text_lines = [x.strip() for x in fp.readlines()]
    except Exception:
        current_dependency_text_lines = []

    # only write the output file if it is different from the current version,
    # to avoid unneccessary dependency triggers during make.
    if dependency_text_lines != current_dependency_text_lines:
        with open(options.makefiledeps, 'w', encoding='utf-8') as fp:
            for line in dependency_text_lines:
                fp.write(f'{line}\n')
        print(f"{options.makefiledeps} file is updated.", file=sys.stderr)
    else:
        print(f"{options.makefiledeps} file is upto date.", file=sys.stderr)


if __name__ == '__main__':
    makedeps()
