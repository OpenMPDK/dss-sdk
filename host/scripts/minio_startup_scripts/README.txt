Requirements:
Python v3 and above.

Install dependent python modules
python3.6 -m pip install -r requirements.txt

Update configuration file ( config.json ):
- Update NKV library path
- Update NKV configuration file path
- Update Minio binary path

Run the script:
- Lunch Minio in non distrinuted mode:
  python3.6  minio.py -c config.json -cm

- Lunch distributed Minio:
  python3.6  minio.py -c config.json --minio_distributed -cm
- Dump ClusterMap:
  Add -cm switch to dump cluster map.
