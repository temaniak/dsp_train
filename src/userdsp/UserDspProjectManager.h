#pragma once

#include <JuceHeader.h>

#include <memory>
#include <vector>

#include "userdsp/UserDspControllers.h"

enum class UserDspProjectNodeType
{
    folder,
    file
};

struct UserDspProjectNode
{
    UserDspProjectNodeType type = UserDspProjectNodeType::folder;
    juce::String name;
    juce::String content;
    UserDspProjectNode* parent = nullptr;
    std::vector<std::unique_ptr<UserDspProjectNode>> children;
};

struct UserDspProjectFileSnapshot
{
    juce::String relativePath;
    juce::String content;
};

struct UserDspProjectSnapshot
{
    juce::String projectName;
    juce::String processorClassName;
    juce::String activeFilePath;
    std::vector<UserDspControllerDefinition> controllers;
    std::vector<UserDspProjectFileSnapshot> files;
};

class UserDspProjectManager final : private juce::CodeDocument::Listener
{
public:
    UserDspProjectManager();
    ~UserDspProjectManager() override;

    juce::CodeDocument& getDocument() noexcept;
    juce::CPlusPlusCodeTokeniser& getTokeniser() noexcept;

    const UserDspProjectNode& getRootNode() const noexcept;
    const UserDspProjectNode* getNodeForPath(const juce::String& relativePath) const noexcept;

    juce::String getProjectName() const;
    juce::File getArchiveFile() const;
    juce::String getActiveFilePath() const;
    juce::String getProcessorClassName() const;
    const std::vector<UserDspControllerDefinition>& getControllerDefinitions() const noexcept;
    bool hasUnsavedChanges() const noexcept;

    void setProcessorClassName(const juce::String& newProcessorClassName);
    juce::Result addController(UserDspControllerType type);
    juce::Result updateController(int index, const UserDspControllerDefinition& definition);
    juce::Result removeController(int index);
    juce::Result moveController(int sourceIndex, int destinationIndex);

    juce::Result createNewProject();
    juce::Result loadProjectArchive(const juce::File& archiveFile);
    juce::Result saveProject();
    juce::Result saveProjectAs(const juce::File& archiveFile);

    juce::Result selectFile(const juce::String& relativePath);
    juce::Result createFile(const juce::String& parentFolderPath, const juce::String& fileName);
    juce::Result createFolder(const juce::String& parentFolderPath, const juce::String& folderName);
    juce::Result renameItem(const juce::String& relativePath, const juce::String& newName);
    juce::Result moveItem(const juce::String& sourcePath, const juce::String& destinationFolderPath);
    juce::Result deleteItem(const juce::String& relativePath);
    juce::Result importFile(const juce::File& fileToImport, const juce::String& destinationFolderPath, juce::String* importedRelativePath = nullptr);
    juce::Result exportFile(const juce::String& relativePath, const juce::File& outputFile);

    std::vector<juce::String> getAllFolderPaths(const juce::String& excludedRootPath = {}) const;
    juce::StringArray getAllowedMoveDestinations(const juce::String& sourcePath) const;
    juce::String describePath(const juce::String& relativePath) const;

    UserDspProjectSnapshot createCompileSnapshot();

private:
    void codeDocumentTextInserted(const juce::String&, int) override;
    void codeDocumentTextDeleted(int, int) override;

    void resetToNewProjectState();
    void setDirty(bool shouldBeDirty);
    void syncActiveDocumentToProject();
    void loadFileIntoEditor(const juce::String& relativePath);
    juce::String getNodePath(const UserDspProjectNode& node) const;
    UserDspProjectNode* findNodeForPath(const juce::String& relativePath) noexcept;
    UserDspProjectNode* findFolderForPath(const juce::String& relativePath) noexcept;
    const UserDspProjectNode* getFolderForPath(const juce::String& relativePath) const noexcept;
    bool pathExists(const juce::String& relativePath) const;
    bool pathIsDescendantOf(const juce::String& candidatePath, const juce::String& parentPath) const;
    UserDspProjectNode* addFolderNode(UserDspProjectNode& parentNode, const juce::String& folderName);
    UserDspProjectNode* addFileNode(UserDspProjectNode& parentNode, const juce::String& fileName, const juce::String& content);
    void collectFileSnapshots(const UserDspProjectNode& node, std::vector<UserDspProjectFileSnapshot>& files) const;
    void collectFolderPaths(const UserDspProjectNode& node, std::vector<juce::String>& paths) const;
    bool matchesDefaultTemplateProject() const;
    juce::String makeUniqueChildName(const UserDspProjectNode& parentNode, const juce::String& preferredName) const;

    std::unique_ptr<UserDspProjectNode> rootNode;
    juce::String projectName { "Untitled Project" };
    juce::File archiveFile;
    juce::String activeFilePath;
    juce::String processorClassName;
    std::vector<UserDspControllerDefinition> controllerDefinitions;
    bool dirty = false;
    bool suppressDocumentNotifications = false;
    juce::CodeDocument document;
    juce::CPlusPlusCodeTokeniser tokeniser;
};
