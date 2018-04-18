#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

typedef std::string SubscriberId;
class SubscriptionRequest
{
public:
    SubscriptionRequest(SubscriberId subscriberId, std::string subscription)
    {
        subscriberId_m = subscriberId;
        subscription_m = subscription;
    }

    SubscriberId getSubscriberId()
    {
        return subscriberId_m;
    }

    std::string getSubscription()
    {
        return subscription_m;
    }

    friend std::ostream &operator<<(std::ostream &s, const SubscriptionRequest& req)
    {
        return s << "(" << req.subscriberId_m << ", " << req.subscription_m << ")";
    }

private:
    SubscriberId subscriberId_m;
    std::string subscription_m;
};

typedef std::vector<SubscriptionRequest> SubscriptionList;
SubscriptionList subscriptionList_g;

void dumpSubscriptionList(SubscriptionList& list)
{
    for (auto& req : list)
    {
        std::cout << req << std::endl;
    }
}

class TopicTrie
{
public:
    class TrieNode
    {
    public:
        typedef std::unordered_map<std::string, TrieNode*> HashTable;
        typedef HashTable TrieNodes;

        TrieNode()
        {
        }

        TrieNode(std::string topic)
        {
            topic_m = topic;
        }

        ~TrieNode()
        {
            nodes_m.clear();
        }

        bool hasChildNode(std::string topic)
        {
            bool keyExists = (nodes_m.find(topic) != nodes_m.end());
            bool childNodeExists = keyExists && (nodes_m[topic] != nullptr);
            return childNodeExists;
        }

        TrieNode* getChildNode(std::string topic)
        {
            return nodes_m[topic];
        }

        bool createChildNode(std::string topic)
        {
            nodes_m[topic] = new TrieNode(topic);
            return true;
        }

    private:
        std::string topic_m;
        TrieNodes nodes_m;
    };

    TopicTrie()
    {
    }

    void addSubscriptionTokens(std::vector<std::string> subscriptionTokens)
    {
        TrieNode* currNode_p = &root_m;
        for (auto &token : subscriptionTokens)
        {
            if (currNode_p->hasChildNode(token))
            {
                std::cout << "Walked node: " << token << std::endl;
                currNode_p = currNode_p->getChildNode(token);
            }
            else
            {
                if (!currNode_p->createChildNode(token))
                {
                    // Error
                    return;
                }
                std::cout << "Created node: " << token << std::endl;
                currNode_p = currNode_p->getChildNode(token);
            }
        }
    }

private:
    TrieNode root_m;
};

class TopicManager
{
public:
    TopicManager()
    {
    }

    void addSubscription(SubscriptionRequest& req)
    {
        std::cout << "Adding subscription: " << req << std::endl;
        std::vector<std::string> subscriptionTokens = tokenizeSubscription(req.getSubscription());
        trie_m.addSubscriptionTokens(subscriptionTokens);
    }

    void addSubscriptionList(SubscriptionList& list)
    {
        for (auto& req : list)
        {
            addSubscription(req);
        }
    }

private:
    std::vector<std::string> tokenizeSubscription(std::string subscription)
    {
        std::vector<std::string> subscriptionTokens;
        std::string item;
        std::stringstream ss(subscription);
        while (std::getline(ss, item, '/'))
        {
            subscriptionTokens.push_back(item);
        }

        return subscriptionTokens;
    }

    TopicTrie trie_m;
};
TopicManager topicManager_g;

int main(int argc, char* argv[])
{
    // Initialize subscription list
    //
    subscriptionList_g.push_back(SubscriptionRequest("Subcriber1", "a/c/c"));
    subscriptionList_g.push_back(SubscriptionRequest("Subcriber1", "b/b/c"));
    subscriptionList_g.push_back(SubscriptionRequest("Subcriber2", "a/+/b/c"));
    subscriptionList_g.push_back(SubscriptionRequest("Subcriber3", "a/#/c"));
    subscriptionList_g.push_back(SubscriptionRequest("Subcriber4", "b/b/c"));
    subscriptionList_g.push_back(SubscriptionRequest("Subcriber5", "b/#"));

    topicManager_g.addSubscriptionList(subscriptionList_g);

    return 0;
}
