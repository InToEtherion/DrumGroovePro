#include "FavoritesManager.h"

FavoritesManager::FavoritesManager()
{
    load();
}

FavoritesManager::~FavoritesManager()
{
    save();
}

void FavoritesManager::addFavorite(const juce::File& folder, const juce::String& customName)
{
    if (!folder.exists() || !folder.isDirectory())
        return;
    
    if (isFavorite(folder))
        return;
    
    juce::String name = customName.isEmpty() ? folder.getFileName() : customName;
    favorites.add(Favorite(name, folder));
    save();
}

void FavoritesManager::removeFavorite(const juce::String& id)
{
    for (int i = 0; i < favorites.size(); ++i)
    {
        if (favorites[i].id == id)
        {
            favorites.remove(i);
            save();
            return;
        }
    }
}

void FavoritesManager::renameFavorite(const juce::String& id, const juce::String& newName)
{
    for (auto& fav : favorites)
    {
        if (fav.id == id)
        {
            fav.name = newName;
            save();
            return;
        }
    }
}

Favorite FavoritesManager::getFavorite(int index) const
{
    if (index >= 0 && index < favorites.size())
        return favorites[index];
    return Favorite();
}

juce::String FavoritesManager::getFavoriteName(int index) const
{
    if (index >= 0 && index < favorites.size())
        return favorites[index].name;
    return "";
}

juce::File FavoritesManager::getFavoritePath(int index) const
{
    if (index >= 0 && index < favorites.size())
        return favorites[index].path;
    return juce::File();
}

juce::String FavoritesManager::getFavoriteId(int index) const
{
    if (index >= 0 && index < favorites.size())
        return favorites[index].id;
    return "";
}

bool FavoritesManager::isFavorite(const juce::File& folder) const
{
    for (const auto& fav : favorites)
    {
        if (fav.path == folder)
            return true;
    }
    return false;
}

void FavoritesManager::save()
{
    auto file = getFavoritesFile();
    
    juce::XmlElement xml("Favorites");
    
    for (const auto& fav : favorites)
    {
        auto* favXml = xml.createNewChildElement("Favorite");
        favXml->setAttribute("id", fav.id);
        favXml->setAttribute("name", fav.name);
        favXml->setAttribute("path", fav.path.getFullPathName());
    }
    
    xml.writeTo(file);
}

void FavoritesManager::load()
{
    auto file = getFavoritesFile();
    
    if (!file.existsAsFile())
        return;
    
    auto xml = juce::parseXML(file);
    if (!xml)
        return;
    
    favorites.clear();
    
    for (auto* favXml : xml->getChildIterator())
    {
        if (favXml->hasTagName("Favorite"))
        {
            Favorite fav;
            fav.id = favXml->getStringAttribute("id");
            fav.name = favXml->getStringAttribute("name");
            fav.path = juce::File(favXml->getStringAttribute("path"));
            
            if (fav.path.exists())
                favorites.add(fav);
        }
    }
}

juce::File FavoritesManager::getFavoritesFile() const
{
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    auto pluginDir = appData.getChildFile("DrumGroovePro");
    
    if (!pluginDir.exists())
        pluginDir.createDirectory();
    
    return pluginDir.getChildFile("favorites.xml");
}