#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

typedef std::string SubscriberId;
class SubscriptionRequest
{
public:
    SubscriptionRequest(SubscriberId subscriberId, std::string topic)
    {
        subscriberId_m = subscriberId;
        topic_m = topic;
    }

    SubscriberId getSubscriberId()
    {
        return subscriberId_m;
    }

    std::string getTopic()
    {
        return topic_m;
    }

    friend std::ostream& operator<<(
            std::ostream& s,
            const SubscriptionRequest& req)
    {
        return s << "(" << req.subscriberId_m << ", " << req.topic_m << ")";
    }

private:
    SubscriberId subscriberId_m;
    std::string topic_m;
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

class TopicStore
{
public:
    virtual void addTopicTokens(
            SubscriberId subscriberId,
            const std::vector<std::string>& topicTokens) = 0;

    virtual std::vector<SubscriberId> getSubscriptionMatches(
            const std::vector<std::string>& topicTokens) = 0;
};

class TopicTrie : public TopicStore
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

        std::string getTopic()
        {
            return topic_m;
        }

        void setTopic(std::string topic)
        {
            topic_m = topic;
        }

        void addSubscriberId(SubscriberId id)
        {
            subscriberIds_m.push_back(id);
        }

        std::vector<SubscriberId> getSubscriberIds()
        {
            return subscriberIds_m;
        }

    private:
        std::string topic_m;
        std::vector<SubscriberId> subscriberIds_m;
        TrieNodes nodes_m;
    };

    TopicTrie()
    {
    }

    void addTopicTokens(
            SubscriberId subscriberId,
            const std::vector<std::string>& topicTokens)
    {
        TrieNode* currNode_p = &root_m;
        for (auto it = topicTokens.begin(); it != topicTokens.end(); it++)
        {
            std::string token = *it;
            if (currNode_p->hasChildNode(token))
            {
                //std::cout << "Walked node: " << token << std::endl;
                currNode_p = currNode_p->getChildNode(token);
            }
            else
            {
                if (!currNode_p->createChildNode(token))
                {
                    // Error
                    return;
                }
                //std::cout << "Created node: " << token << std::endl;
                currNode_p = currNode_p->getChildNode(token);
            }

            // Mark the node with the subscriber ID if this is the last token in
            // the subscription string
            //
            if (it == (topicTokens.end()-1))
            {
                //std::cout << "Adding subscriber (" << subscriberId << ") to node (" << currNode_p->getTopic() << ")" << std::endl;
                currNode_p->addSubscriberId(subscriberId);
            }
        }
    }

    std::vector<SubscriberId> getSubscriptionMatches(
            const std::vector<std::string>& topicTokens)
    {
        std::vector<SubscriberId> matches;
        TrieNode* currNode_p = &root_m;
        std::vector<std::string>::const_iterator it = topicTokens.begin();
        walkTrieGetMatches(currNode_p, it, matches, topicTokens);
        return matches;
    }

private:
    void walkTrieGetMatches(TrieNode* currNode_p,
                            std::vector<std::string>::const_iterator it,
                            std::vector<SubscriberId>& matches,
                            const std::vector<std::string>& topicTokens)
    {
        // End of the tokens
        //
        if (it == topicTokens.end())
        {
            // This handles the case where there is a multi-level wildcard child
            // node one level past the end of the topic tokens. This case
            // results in a match because the multi-level wildcard can match
            // "0" or more topics.
            //
            if (currNode_p->hasChildNode("#"))
            {
                handleMultiLevelWildcardChildNode(currNode_p, matches);
            }

            // This handles the case where there the last single-level wildcard
            //if (currNode_p->getTopic() == "+")
            return;
        }

        std::string token = *it;

        // If the current node has a multi-level wildcard child node, then all
        // subscribers subscribed to that node matches the current pattern.
        //
        if (currNode_p->hasChildNode("#"))
        {
            handleMultiLevelWildcardChildNode(currNode_p, matches);
        }

        // If the current node has a single-level wildcard child node, things
        // get a little trickier... there are two cases:
        //
        // 1. The single-level wildcard node matched the last topic token:
        //
        //   Add all subscribers of that node to the list of matches
        //
        // 2. The single-level wildcard node didn't match the last topic token:
        //
        //   We want to continue to walk the trie in its own seperate state so
        //   we can backtrack to the current state to finish walking the trie.
        //
        if (currNode_p->hasChildNode("+"))
        {
            std::cout << "Walked node: +" << std::endl;
            TrieNode* slwcNode_p = currNode_p->getChildNode("+");
            std::vector<std::string>::const_iterator nextTokenIt = it+1;

            // If the single-level wildcard matched the last topic token, then
            // add all subscribers of that node to the list of matches
            //
            if (nextTokenIt == topicTokens.end())
            {
                addNodeSubscriptionsToMatches(slwcNode_p, matches);
            }
            else
            {
                walkTrieGetMatches(slwcNode_p, nextTokenIt, matches, topicTokens);
            }
        }

        if (!currNode_p->hasChildNode(token) && (token != "+"))
        {
            return;
        }

        std::cout << "Walked node: " << token << std::endl;
        currNode_p = currNode_p->getChildNode(token);

        // If this is the last token in the subscription string, then add all
        // subscribers of this node to the list of matches
        //
        if (it == (topicTokens.end()-1))
        {
            addNodeSubscriptionsToMatches(currNode_p, matches);
        }

        walkTrieGetMatches(currNode_p, ++it, matches, topicTokens);
    }

    void handleMultiLevelWildcardChildNode(
            TrieNode* currNode_p,
            std::vector<SubscriberId>& matches)
    {
        std::cout << "Walked node: #" << std::endl;
        TrieNode* mlwcNode_p = currNode_p->getChildNode("#");
        addNodeSubscriptionsToMatches(mlwcNode_p, matches);
    }

    void addNodeSubscriptionsToMatches(
            TrieNode* node_p,
            std::vector<SubscriberId>& matches)
    {
        std::vector<SubscriberId> subscriberIds = node_p->getSubscriberIds();
        for (auto id : subscriberIds)
        {
            matches.push_back(id);
            std::cout << "Match id: " << id << std::endl;
        }
    }

    TrieNode root_m;
};

class TopicManager
{
public:
    TopicManager() :
        store_mp(new TopicTrie())
    {
    }

    ~TopicManager()
    {
        if (store_mp != nullptr) delete [] store_mp;
    }

    void addSubscription(SubscriptionRequest& req)
    {
        std::cout << "Adding subscription: " << req << std::endl;
        std::vector<std::string> topicTokens = tokenizeTopic(req.getTopic());
        store_mp->addTopicTokens(req.getSubscriberId(), topicTokens);
    }

    void addSubscriptionList(SubscriptionList& list)
    {
        for (auto& req : list)
        {
            addSubscription(req);
        }
    }

    std::vector<SubscriberId> getSubscriptionMatches(std::string topic)
    {
        std::cout << "Getting matches for topic: " << topic << std::endl;
        std::vector<std::string> topicTokens = tokenizeTopic(topic);
        return store_mp->getSubscriptionMatches(topicTokens);
    }

private:
    std::vector<std::string> tokenizeTopic(std::string topic)
    {
        std::vector<std::string> topicTokens;
        std::string item;
        std::stringstream ss(topic);
        while (std::getline(ss, item, '/'))
        {
            topicTokens.push_back(item);
        }

        return topicTokens;
    }

    TopicStore* store_mp;
};
TopicManager topicManager_g;

int main(int argc, char* argv[])
{
    // Initialize subscription list
    //
    subscriptionList_g.push_back(SubscriptionRequest("Subcriber01", "a/c/c"));
    subscriptionList_g.push_back(SubscriptionRequest("Subcriber01", "b/b/c"));
    subscriptionList_g.push_back(SubscriptionRequest("Subcriber02", "a/+/b/c"));
    subscriptionList_g.push_back(SubscriptionRequest("Subcriber03", "a/#"));
    subscriptionList_g.push_back(SubscriptionRequest("Subcriber04", "b/b/c"));
    subscriptionList_g.push_back(SubscriptionRequest("Subcriber05", "b/#"));
    subscriptionList_g.push_back(SubscriptionRequest("Subcriber06", "+"));
    subscriptionList_g.push_back(SubscriptionRequest("Subcriber07", "+/+"));
    subscriptionList_g.push_back(SubscriptionRequest("Subcriber08", "+/a"));
    subscriptionList_g.push_back(SubscriptionRequest("Subcriber09", "#"));
    subscriptionList_g.push_back(SubscriptionRequest("Subcriber10", "b/+/c"));
    subscriptionList_g.push_back(SubscriptionRequest("Subcriber11", "+/+/c"));
    subscriptionList_g.push_back(SubscriptionRequest("Subcriber12", "+/#"));
    subscriptionList_g.push_back(SubscriptionRequest("Subcriber13", "b/b/#"));
    subscriptionList_g.push_back(SubscriptionRequest("Subcriber14", "b/b/c/#"));
    subscriptionList_g.push_back(SubscriptionRequest("Subcriber15", "b/b/+"));
    subscriptionList_g.push_back(SubscriptionRequest("Subcriber16", "b/b/c/+"));

    topicManager_g.addSubscriptionList(subscriptionList_g);

    std::string publishedTopic = "b/b/c";
    std::vector<std::string> matches = topicManager_g.getSubscriptionMatches(publishedTopic);
    std::cout << "Matches: ( ";
    for (auto& match : matches)
    {
        std::cout << match << " ";
    }
    std::cout << ")" << std::endl;

    return 0;
}
