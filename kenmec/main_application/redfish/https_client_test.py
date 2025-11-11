import requests
from requests_toolbelt.adapters.host_header_ssl import HostHeaderSSLAdapter

session = requests.Session()
session.mount("https://", HostHeaderSSLAdapter())

# Connect to the IP but send Host/SNI = Dexatek.local
r = session.get(
    "https://192.168.1.231:8443/redfish/v1",
    headers={"Host": "Dexatek.local"},     # Host header (also used for SNI by the adapter)
    cert=("redfish_certification/client/client.crt",
          "redfish_certification/client/client.key"),  # mTLS
    verify="redfish_certification/root_ca/ca.crt",     # server CA
    timeout=10,
)
print(r.status_code)
print(r.text)
