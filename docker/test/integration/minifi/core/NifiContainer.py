import logging

from .FlowContainer import FlowContainer
from ..flow_serialization.Nifi_flow_xml_serializer import Nifi_flow_xml_serializer
import gzip
import os


class NifiContainer(FlowContainer):
    NIFI_VERSION = '1.7.0'
    NIFI_ROOT = '/opt/nifi/nifi-' + NIFI_VERSION

    def __init__(self, config_dir, name, vols, network, image_store, command=None):
        if not command:
            entry_command = (r"sed -i -e 's/^\(nifi.remote.input.host\)=.*/\1={name}/' {nifi_root}/conf/nifi.properties && "
                             r"cp /tmp/nifi_config/flow.xml.gz {nifi_root}/conf && /opt/nifi/scripts/start.sh").format(name=name, nifi_root=NifiContainer.NIFI_ROOT)
            command = ["/bin/sh", "-c", entry_command]
        super().__init__(config_dir, name, 'nifi', vols, network, image_store, command)

    def get_startup_finished_log_entry(self):
        return "Starting Flow Controller"

    def get_log_file_path(self):
        return NifiContainer.NIFI_ROOT + '/logs/nifi-app.log'

    def __create_config(self):
        serializer = Nifi_flow_xml_serializer()
        test_flow_xml = serializer.serialize(self.start_nodes, NifiContainer.NIFI_VERSION)
        logging.info('Using generated flow config xml:\n%s', test_flow_xml)

        with gzip.open(os.path.join(self.config_dir, "flow.xml.gz"), 'wb') as gz_file:
            gz_file.write(test_flow_xml.encode())

    def deploy(self):
        if not self.set_deployed():
            return

        logging.info('Creating and running nifi docker container...')
        self.__create_config()
        self.client.containers.run(
            self.image_store.get_image(self.get_engine()),
            detach=True,
            name=self.name,
            hostname=self.name,
            network=self.network.name,
            entrypoint=self.command,
            volumes=self.vols)
        logging.info('Added container \'%s\'', self.name)
