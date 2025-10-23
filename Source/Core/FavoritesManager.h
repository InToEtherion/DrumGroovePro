#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

struct Favorite
{
    juce::String name;
    juce::File path;
    juce::String id;
    
    Favorite() = default;
    Favorite(const juce::String& n, const juce::File& p)
        : name(n), path(p), id(juce::Uuid().toString()) {}
};

class FavoritesManager
{
public:
    FavoritesManager();
    ~FavoritesManager();
    
    void addFavorite(const juce::File& folder, const juce::String& customName = "");
    void removeFavorite(const juce::String& id);
    void renameFavorite(const juce::String& id, const juce::String& newName);
    
    int getNumFavorites() const { return favorites.size(); }
    Favorite getFavorite(int index) const;
    juce::String getFavoriteName(int index) const;
    juce::File getFavoritePath(int index) const;
    juce::String getFavoriteId(int index) const;
    bool isFavorite(const juce::File& folder) const;
    
    void save();
    void load();
    
private:
    juce::Array<Favorite> favorites;
    juce::File getFavoritesFile() const;
};