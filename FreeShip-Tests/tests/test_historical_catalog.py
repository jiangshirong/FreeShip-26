import copy
import json
from pathlib import Path
import sys
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from historical_regression import validate_catalog

class HistoricalCatalogTests(unittest.TestCase):
    def setUp(self):
        self.entries=json.loads((ROOT/'historical_manifest.json').read_text('utf8'))['entries']
    def test_complete_reviewed_catalog(self):
        validate_catalog(ROOT,self.entries)
    def test_missing_entry_is_rejected(self):
        with self.assertRaises(RuntimeError):validate_catalog(ROOT,self.entries[1:])
    def test_changed_source_is_rejected(self):
        self.entries[0]['sha256']='0'*64
        with self.assertRaises(RuntimeError):validate_catalog(ROOT,self.entries)
    def test_archive_alone_is_not_integration(self):
        entry=next(e for e in self.entries if e['disposition']=='integrated')
        entry['path']=entry['source']
        with self.assertRaises(RuntimeError):validate_catalog(ROOT,self.entries)
    def test_unreviewed_disposition_is_rejected(self):
        self.entries[0]['disposition']='review'
        with self.assertRaises(RuntimeError):validate_catalog(ROOT,self.entries)

if __name__=='__main__':unittest.main()
