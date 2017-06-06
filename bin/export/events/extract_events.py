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
import logging
from optparse import OptionParser
from ga_event2qml import event_xml, setup_event2qml, version

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


def extract_event(db_path, ev_file):
    """
    :param db_path: database location 
    :param ev_file: events file name, csv file
    
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


def extract_all_events(ev, qml, db_path, output_dir):
    with ds.closing(ds.dbopen(db_path, 'r')) as db:
        with ds.freeing(db.process(['dbopen event', 'dbsort evid'])) as view:
            for row in view.iter_record():
                log.info('Processing event' +
                         ' '.join([str(row.getv(x)[0]) for x in EVENT_FIELDS]))
                event_id = row.getv(EVENT_FIELDS[0])[0]
                event_xml(event_id=event_id,
                          event=ev,
                          quakeml=qml,
                          output_file=os.path.join(output_dir, str(event_id)))


if __name__ == '__main__':

    parser = OptionParser(usage=usage,
                          version="%prog " + version,
                          description=description)

    # Set schema file
    parser.add_option("-s", action="store", dest="schema",
                      default='', help="XML Schema Definition to implement")

    # Vebose output
    parser.add_option("-v", action="store_true", dest="verbose",
                      default=False, help="run with verbose output")

    # Debug output
    parser.add_option("-d", action="store_true", dest="debug",
                      default=False, help="run with debug output")

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

    ev, qml = setup_event2qml(options=options, database=args[0])

    DB_PATH = os.path.abspath(args[0])
    assert os.path.exists(DB_PATH), 'provide correct path to db dir.'

    outdir_arg = args[1] if len(args) > 1 else 'outdir'

    outdir = os.path.join(os.getcwd(), outdir_arg)

    if not os.path.exists(outdir):
        os.mkdir(outdir)
    else:
        parser.error("Specified output dir '{}' exists.\n"
                     "Remove output dir and try again.".format(outdir_arg))

    extract_all_events(ev, qml, DB_PATH, output_dir=outdir)
