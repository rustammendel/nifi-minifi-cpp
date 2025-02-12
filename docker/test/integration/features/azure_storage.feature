Feature: Sending data from MiNiFi-C++ to an Azure storage server
  In order to transfer data to interact with Azure servers
  As a user of MiNiFi
  I need to have a PutAzureBlobStorage processor

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: A MiNiFi instance can upload data to Azure blob storage
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "#test_data$123$#" is present in "/tmp/input"
    And a PutAzureBlobStorage processor set up to communicate with an Azure blob storage
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PutAzureBlobStorage
    And the "success" relationship of the PutAzureBlobStorage processor is connected to the PutFile

    And an Azure storage server is set up

    When all instances start up

    Then a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds
    And the object on the Azure storage server is "#test_data$123$#"

  Scenario: A MiNiFi instance can delete blob from Azure blob storage
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a DeleteAzureBlobStorage processor set up to communicate with an Azure blob storage
    And the "Blob" property of the DeleteAzureBlobStorage processor is set to "test"
    And the "success" relationship of the GenerateFlowFile processor is connected to the DeleteAzureBlobStorage
    And an Azure storage server is set up

    When all instances start up
    And test blob "test" is created on Azure blob storage

    Then the Azure blob storage becomes empty in 30 seconds

  Scenario: A MiNiFi instance can delete blob from Azure blob storage including snapshots
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a DeleteAzureBlobStorage processor set up to communicate with an Azure blob storage
    And the "Blob" property of the DeleteAzureBlobStorage processor is set to "test"
    And the "Delete Snapshots Option" property of the DeleteAzureBlobStorage processor is set to "Include Snapshots"
    And the "success" relationship of the GenerateFlowFile processor is connected to the DeleteAzureBlobStorage
    And an Azure storage server is set up

    When all instances start up
    And test blob "test" is created on Azure blob storage with a snapshot

    Then the Azure blob storage becomes empty in 30 seconds

  Scenario: A MiNiFi instance can delete blob snapshots from Azure blob storage
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a DeleteAzureBlobStorage processor set up to communicate with an Azure blob storage
    And the "Blob" property of the DeleteAzureBlobStorage processor is set to "test"
    And the "Delete Snapshots Option" property of the DeleteAzureBlobStorage processor is set to "Delete Snapshots Only"
    And the "success" relationship of the GenerateFlowFile processor is connected to the DeleteAzureBlobStorage
    And an Azure storage server is set up

    When all instances start up
    And test blob "test" is created on Azure blob storage with a snapshot

    Then the blob and snapshot count becomes 1 in 30 seconds
