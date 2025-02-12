import logging
from .Container import Container


class AzureStorageServerContainer(Container):
    def __init__(self, name, vols, network, image_store, command=None):
        super().__init__(name, 'azure-storage-server', vols, network, image_store, command)

    def get_startup_finished_log_entry(self):
        return "Azurite Queue service is successfully listening at"

    def deploy(self):
        if not self.set_deployed():
            return

        logging.info('Creating and running azure storage server docker container...')
        self.client.containers.run(
            "mcr.microsoft.com/azure-storage/azurite:3.14.2",
            detach=True,
            name=self.name,
            network=self.network.name,
            ports={'10000/tcp': 10000, '10001/tcp': 10001},
            entrypoint=self.command)
        logging.info('Added container \'%s\'', self.name)
