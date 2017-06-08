"""
This script will dump all antelope events from antelope database.
The database dir location has to be provided.
An optional events.csv filename can also be provided.
If the csv file is not provided, it is saved as events.csv.
"""
from __future__ import print_function
import os
import sys
import csv
import subprocess
import logging
from optparse import OptionParser
from ga_event2qml import version
from obspy.core.event import read_events


logging.basicConfig(level=logging.INFO)
log = logging.getLogger(__name__)

description = """
GA adapted QuakeML export infrastructure for Antelope
---------------------------------------------------------------------------
The original quakeml export was adapted to work with GA's infrastructure.

Sudipta Basak
basaks@gmail.com
---------------------------------------------------------------------------
"""


sys.path.append(os.environ['ANTELOPE'] + '/data/python')
import antelope.datascope as ds

EVENT_FIELDS = ['evid', 'prefor', 'auth']
DEFAULT_EVENT_FILE = 'events.csv'

usage = "\n\t\tevent2qml [-h] [-v] [-d] [-p pfname] [-s XSD_schema] " \
            "database [OUT_DIR] \n"


def _extract_events(db_path, ev_file):
    """
    :param db_path: str, database location 
    :param ev_file: str, events file name, csv file    
    """
    ev_file = ev_file if ev_file else DEFAULT_EVENT_FILE
    with ds.closing(ds.dbopen(db_path, 'r')) as db:
        with ds.freeing(db.process(['dbopen event', 'dbsort evid'])) as view:
            with open(ev_file, 'w') as csv_file:
                writer = csv.writer(csv_file, delimiter=',')
                csv_file.write(','.join(EVENT_FIELDS) + '\n')
                for row in view.iter_record():
                    writer.writerow([str(row.getv(x)[0]) for x in
                                     EVENT_FIELDS])


def _extract_all_events(cmd, output_dir, ev_file):
    """
    We repeatedly call dbopen to prevent memory accumulation during the 
    repeated calls to database while exporting events quakeml file.
    :param cmd: str, command string 
    :param output_dir: str, dir where quakeml files should be saved  
    :param ev_file: str, event file location
    :return: 
    """
    with open(ev_file, 'r') as csv_file:
        reader = csv.reader(csv_file, delimiter=',')
        reader.next()
        for i, r in enumerate(reader):
            event_id = r[0]
            log.info('==========Processing event #{} '.format(i + 1) +
                     'with event id #{}================'.format(event_id))
            event_qml = os.path.join(output_dir, str(event_id) + '.xml')
            ev_cmd = cmd + event_id + ' -o {}'.format(event_qml)
            subprocess.check_call(ev_cmd, shell=True)
            log.info('==========Finished processing event #{} '.format(i + 1) +
                     'with event id #{}================'.format(event_id))


def _convert_to_sc3xml(output_dir, sc3xml):
    # read in all the QuakeML files into a catalogue object
    catalog = read_events(pathname_or_url=os.path.join(output_dir, '*.xml'),
                          format='QUAKEML')
    # now write out seiscomp3ML
    catalog.write(filename=sc3xml, format='SC3ML')


if __name__ == '__main__':

    parser = OptionParser(usage=usage,
                          version="%prog " + version,
                          description=description)

    # Set schema file
    parser.add_option("-s", action="store", dest="schema",
                      default='', help="XML Schema Definition to implement")

    # Verbose output
    parser.add_option("-v", action="store_true", dest="verbose",
                      default=False, help="run with verbose output")

    # Debug output
    parser.add_option("-d", action="store_true", dest="debug",
                      default=False, help="run with debug output")

    # Output seiscomp3 XML file
    parser.add_option("-o", action="store", dest="output_file",
                      default=False, help="Output seiscomp3 xml file")

    # Parameter File
    parser.add_option("-p", action="store", dest="pf",
                      default='event2qml.pf', help="parameter file to use")

    (options, args) = parser.parse_args()

    # If we don't have 2 arguments then exit.
    if len(args) < 1 or len(args) > 2:
        parser.print_help()
        parser.error("incorrect number of arguments")

    # Set log level
    loglevel = 'WARNING'
    if options.debug:
        loglevel = 'DEBUG'
    elif options.verbose:
        loglevel = 'INFO'

    log.info(parser.get_version())
    log.setLevel(level=loglevel)
    log.info('loglevel=%s' % loglevel)

    DB_PATH = os.path.abspath(args[0])
    assert os.path.exists(DB_PATH), 'provide correct path to db dir.'

    outdir_arg = 'outdir'

    outdir = os.path.join(os.getcwd(), outdir_arg)

    if not os.path.exists(outdir):
        os.mkdir(outdir)
    else:
        parser.error("Specified output dir '{}' exists.\n"
                     "Remove output dir and try again.".format(outdir_arg))

    ev_file = os.path.join(outdir, 'events.csv')
    _extract_events(db_path=DB_PATH, ev_file=ev_file)

    cmd = 'python ga_event2qml.py ' \
          '-s {} ' \
          '-d ' \
          '{} '.format(options.schema, DB_PATH)

    _extract_all_events(cmd, output_dir=outdir, ev_file=ev_file)
    _convert_to_sc3xml(output_dir=outdir, sc3xml=options.output_file)
