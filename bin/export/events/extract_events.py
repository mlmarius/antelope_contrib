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

sys.path.append(os.environ['ANTELOPE'] + '/data/python')
import antelope.datascope as ds

EVENT_FIELDS = ['evid', 'prefor', 'auth']
DEFAULT_EVENT_FILE = 'events.csv'


def extract_views(db_path, ev_file):
    ev_file = ev_file if ev_file else DEFAULT_EVENT_FILE
    with ds.closing(ds.dbopen(db_path, 'r')) as db:
        with ds.freeing(db.process(['dbopen event', 'dbsort evid'])) as view:
            with open(ev_file, 'w') as csv_file:
                writer = csv.writer(csv_file, delimiter=',')
                csv_file.write(','.join(EVENT_FIELDS) + '\n')
                for row in view.iter_record():
                    writer.writerow([str(row.getv(x)[0]) for x in
                                     EVENT_FIELDS])


def print_usage():
    print('Usage:\n\t{}\n\tor\n\t{}\n'.format(
        'python extract_events.py db_dir',
        'python extract_events.py db_dir output_file'))


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print_usage()
        sys.exit(0)

    DB_DIR = os.path.abspath(sys.argv[1])
    DB_PATH = os.path.join(DB_DIR, 'ansn')
    assert os.path.exists(DB_PATH), 'provide correct path to db dir.'
    if len(sys.argv) > 2:
        events_file = os.path.abspath(sys.argv[2])
    else:
        events_file = False

    extract_views(DB_PATH, ev_file=events_file)



