"""Separate installation identity for the .749-based Pokemon preview."""
import re
import xml.etree.ElementTree as ET

BASE_NAME = 'TwilightPrincessRandomizer'
PREVIEW_NAME = 'DusklightPokemonPreview'
PUBLISHER = 'CN=TwilightPrincessRandomizer'
DISPLAY_NAME = 'Dusklight Pokémon Preview'
PHONE_ID = 'd101f7c3-b4cd-53d7-8d13-d35f8594dd95'
NS = {
    'm': 'http://schemas.microsoft.com/appx/manifest/foundation/windows10',
    'mp': 'http://schemas.microsoft.com/appx/2014/phone/manifest',
    'uap': 'http://schemas.microsoft.com/appx/manifest/uap/windows10',
    'rescap': 'http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities',
    'build': 'http://schemas.microsoft.com/developer/appx/2015/build',
}


def preview_manifest(xml, version):
    assert re.fullmatch(r'\d+\.\d+\.\d+\.\d+', version)
    for prefix, namespace in NS.items():
        ET.register_namespace('' if prefix == 'm' else prefix, namespace)
    root = ET.fromstring(xml)
    identity = root.find('m:Identity', NS)
    assert identity is not None
    assert identity.attrib == {'Name': BASE_NAME, 'Publisher': PUBLISHER,
                              'Version': '1.4.1.749', 'ProcessorArchitecture': 'x64'}
    identity.set('Name', PREVIEW_NAME)
    identity.set('Version', version)
    phone = root.find('mp:PhoneIdentity', NS)
    assert phone is not None and phone.get('PhoneProductId') != PHONE_ID
    phone.set('PhoneProductId', PHONE_ID)
    display = root.find('m:Properties/m:DisplayName', NS)
    visual = root.find('m:Applications/m:Application/uap:VisualElements', NS)
    assert display is not None and visual is not None
    display.text = DISPLAY_NAME
    visual.set('DisplayName', DISPLAY_NAME)
    visual.set('Description', 'Separate Pikachu preview built from Dusklight Xbox .749')
    # Retain executable, entry point, publisher, dependencies, capabilities and all
    # artwork paths. A different Name gives this app a separate package family.
    result = ET.tostring(root, encoding='unicode', xml_declaration=True)
    assert PREVIEW_NAME != BASE_NAME
    return result + '\n'
