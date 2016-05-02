/*
 * DynamicNodeOperationsUtil.h
 *
 *  Created on: Apr 7, 2016
 *      Author: max
 */

#ifndef SRC_UTIL_DYNAMICNODEOPERATIONSUTIL_H_
#define SRC_UTIL_DYNAMICNODEOPERATIONSUTIL_H_

#include "nodes/NodeAddressContent.h"

template <unsigned int DIM>
class Entry;
template <unsigned int DIM>
class Node;

template <unsigned int DIM>
class DynamicNodeOperationsUtil {
public:

	static Node<DIM>* insert(const Entry<DIM>* e, Node<DIM>* rootNode, size_t dim, size_t bitLength);

private:
	static inline void createSubnodeWithExistingSuffix(size_t dim, size_t bitLength,
			size_t currentIndex, Node<DIM>* currentNode, NodeAddressContent<DIM>& content,
			const Entry<DIM>* entry);
	static inline Node<DIM>* insertSuffix(size_t dim,  size_t bitLength, size_t currentIndex,
			size_t hcAddress, Node<DIM>* currentNode,  const Entry<DIM>* entry);
	static inline void splitSubnodePrefix(size_t dim, size_t bitLength,
			size_t currentIndex, size_t differentAtIndex, Node<DIM>* currentNode,
			NodeAddressContent<DIM>& content, const Entry<DIM>* entry);
};

#include <assert.h>
#include <stdexcept>
#include "util/SpatialSelectionOperationsUtil.h"
#include "util/NodeTypeUtil.h"
#include "util/MultiDimBitset.h"
#include "nodes/Node.h"
#include "nodes/NodeAddressContent.h"

using namespace std;

#define DEBUG false

template <unsigned int DIM>
void DynamicNodeOperationsUtil<DIM>::createSubnodeWithExistingSuffix(size_t dim,
		size_t bitLength, size_t currentIndex, Node<DIM>* currentNode,
		NodeAddressContent<DIM>& content, const Entry<DIM>* entry) {
	Node<DIM>* subnode = NodeTypeUtil::determineNodeType<DIM>(bitLength, 2);

	// set longest common prefix in subnode
	const size_t prefixLength = entry->values_.calculateLongestCommonPrefix(currentIndex + 1, content.suffix, &subnode->prefix_);

	// address in subnode starts after common prefix
	const long insertEntryHCAddress = entry->values_.interleaveBits(currentIndex + 1 + prefixLength);
	const long existingEntryHCAddress = content.suffix->interleaveBits(prefixLength);
	assert(insertEntryHCAddress != existingEntryHCAddress); // otherwise there would have been a longer prefix

	// add remaining bits after prefix and addresses as suffixes
	const size_t suffixLength = bitLength - (currentIndex + 1 + prefixLength + 1);
	MultiDimBitset<DIM>* insertEntrySuffix = subnode->insertAtAddress(insertEntryHCAddress, suffixLength, entry->id_);
	entry->values_.removeHighestBits(currentIndex + 1 + prefixLength + 1, insertEntrySuffix);
	MultiDimBitset<DIM>* exisitingEntrySuffix = subnode->insertAtAddress(existingEntryHCAddress, suffixLength, content.id);
	content.suffix->removeHighestBits(prefixLength + 1, exisitingEntrySuffix);
	currentNode->insertAtAddress(content.address, subnode);

	// no need to adjust size because the correct node type was already provided
	assert (currentNode->lookup(content.address).subnode == subnode);
	assert (*subnode->lookup(existingEntryHCAddress).suffix == *exisitingEntrySuffix);
	assert (*subnode->lookup(insertEntryHCAddress).suffix == *insertEntrySuffix);
	assert (subnode->lookup(existingEntryHCAddress).suffix->getBitLength() == suffixLength);
	assert (subnode->lookup(insertEntryHCAddress).suffix->getBitLength() == suffixLength);
}

template <unsigned int DIM>
Node<DIM>* DynamicNodeOperationsUtil<DIM>::insertSuffix(size_t dim, size_t bitLength, size_t currentIndex,
		size_t hcAddress, Node<DIM>* currentNode, const Entry<DIM>* entry) {
	const size_t suffixLength = bitLength - (currentIndex + 1);
	MultiDimBitset<DIM>* suffix = currentNode->insertAtAddress(hcAddress, suffixLength, entry->id_);
	entry->values_.removeHighestBits(currentIndex + 1, suffix);

	Node<DIM>* adjustedNode = currentNode->adjustSize();
	assert(adjustedNode);
	assert(adjustedNode->lookup(hcAddress).exists);
	assert (*adjustedNode->lookup(hcAddress).suffix == *suffix);
	assert (adjustedNode->lookup(hcAddress).suffix->getBitLength() == suffixLength);
	assert (adjustedNode->lookup(hcAddress).id == entry->id_);

	return adjustedNode;
}

template <unsigned int DIM>
void DynamicNodeOperationsUtil<DIM>::splitSubnodePrefix(size_t dim, size_t bitLength,
		size_t currentIndex, size_t differentAtIndex, Node<DIM>* currentNode,
		NodeAddressContent<DIM>& content, const Entry<DIM>* entry) {
	Node<DIM>* oldSubnode = content.subnode;
	Node<DIM>* newSubnode = NodeTypeUtil::determineNodeType<DIM>(bitLength, 2);

	const unsigned long newSubnodeEntryHCAddress = entry->values_.interleaveBits(currentIndex + 1 + differentAtIndex);
	const unsigned long newSubnodePrefixDiffHCAddress = oldSubnode->prefix_.interleaveBits(differentAtIndex);
	assert (newSubnodeEntryHCAddress != newSubnodePrefixDiffHCAddress);

	// move A part of old prefix to new subnode and remove [A | d] from old prefix
	oldSubnode->prefix_.duplicateHighestBits(differentAtIndex, &(newSubnode->prefix_));
	oldSubnode->prefix_.removeHighestBits(differentAtIndex + 1);

	currentNode->insertAtAddress(content.address, newSubnode);
	const size_t suffixLength = bitLength - (currentIndex + 1 + differentAtIndex + 1);
	MultiDimBitset<DIM>* newSubnodeEntryPrefix = newSubnode->insertAtAddress(newSubnodeEntryHCAddress, suffixLength, entry->id_);
	entry->values_.removeHighestBits(currentIndex + 1 + differentAtIndex + 1, newSubnodeEntryPrefix);
	newSubnode->insertAtAddress(newSubnodePrefixDiffHCAddress, oldSubnode);

	// no need to adjust size because the old node remains and the new one already
	// has the correct size
	assert (currentNode->lookup(content.address).hasSubnode);
	assert (currentNode->lookup(content.address).subnode == newSubnode);
	assert (!newSubnode->lookup(newSubnodeEntryHCAddress).hasSubnode);
	assert (*newSubnode->lookup(newSubnodeEntryHCAddress).suffix == *newSubnodeEntryPrefix);
	assert (newSubnode->lookup(newSubnodeEntryHCAddress).suffix->getBitLength() == suffixLength);
	assert (newSubnode->lookup(newSubnodePrefixDiffHCAddress).hasSubnode);
	assert (newSubnode->lookup(newSubnodePrefixDiffHCAddress).subnode == oldSubnode);
}

template <unsigned int DIM>
Node<DIM>* DynamicNodeOperationsUtil<DIM>::insert(const Entry<DIM>* entry, Node<DIM>* rootNode,
		size_t dim, size_t bitLength) {

	size_t depth = 0;
	size_t lastHcAddress = 0;
	size_t index = 0;
	Node<DIM>* lastNode = NULL;
	Node<DIM>* currentNode = rootNode;
	Node<DIM>* initialNode = rootNode;
	NodeAddressContent<DIM> content;

	while (index < bitLength) {
		if (DEBUG)
			cout << "(depth " << depth << "): ";

		const size_t currentIndex = index + currentNode->getPrefixLength();
		const unsigned long hcAddress = entry->values_.interleaveBits(currentIndex);
		// TODO create content once and populate after each iteration instead of creating a new one
		currentNode->lookup(hcAddress, content);
		assert(!content.exists || content.address == hcAddress);
		assert(
				(!content.exists || (content.hasSubnode && content.subnode)
						|| (!content.hasSubnode && content.suffix))
						&& "before insertion there is either a subnode XOR a suffix at the address or the content does not exist");

		if (content.exists && content.hasSubnode) {
			// node entry and subnode exist:
			// validate prefix of subnode
			// case 1 (entry contains prefix): recurse on subnode
			// case 2 (otherwise): split prefix at difference into two subnodes
			pair<bool, size_t> comp = entry->values_.compareTo(currentIndex + 1ul,
					currentIndex + 1ul + content.subnode->prefix_.getBitLength(),
					content.subnode->prefix_);
			bool prefixIncluded = comp.first;
			size_t differentBitAtPrefixIndex = comp.second;

			if (prefixIncluded) {
				// recurse on subnode
				if (DEBUG)
					cout << "recurse -> ";
				lastHcAddress = hcAddress;
				lastNode = currentNode;
				currentNode = content.subnode;
				++depth;
				index = currentIndex + 1;
			} else {
				if (DEBUG)
					cout << "split subnode prefix" << endl;
				// split prefix of subnode [A | d | B] where d is the index of the first different bit
				// create new node with prefix A and only leave prefix B in old subnode
				splitSubnodePrefix(dim, bitLength, currentIndex,
						differentBitAtPrefixIndex, currentNode, content, entry);

				break;
			}

		} else if (content.exists && !content.hasSubnode) {
			if (DEBUG)
				cout << "create subnode with existing suffix" << endl;
			// node entry and suffix exist:
			// convert suffix to new node with prefix (longest common) + insert
			createSubnodeWithExistingSuffix(dim, bitLength, currentIndex,
					currentNode, content, entry);

			break;
		} else {
			if (DEBUG)
				cout << "insert" << endl;
			// node entry does not exist:
			// insert entry + suffix
			Node<DIM>* adjustedNode = insertSuffix(dim, bitLength, currentIndex,
					hcAddress, currentNode, entry);
			assert(adjustedNode);
			if (adjustedNode != currentNode && lastNode) {
				// the subnode changed: store the new one and delete the old
				lastNode->insertAtAddress(lastHcAddress, adjustedNode);
				delete currentNode;
				currentNode = adjustedNode;
			} else if (adjustedNode != currentNode) {
				// the root node changed
				initialNode = adjustedNode;
				currentNode = adjustedNode;
			}

			break;
		}
	}

	#ifndef NDEBUG
	// validation only: lookup again after insertion
	size_t hcAddress = entry->values_.interleaveBits(index + currentNode->getPrefixLength());
	currentNode->lookup(hcAddress, content);
	assert(content.exists && content.address == hcAddress
					&& "after insertion the entry is always contained at the address");
	assert(((content.hasSubnode && content.subnode)
					|| (!content.hasSubnode && content.suffix))
					&& "after insertion there is either a subnode XOR a suffix at the address");
	assert((content.hasSubnode
					|| (index + currentNode->getPrefixLength() + 1
							+ currentNode->getSuffixSize(
									currentNode->lookup(hcAddress))
							== currentNode->valueLength_))
					&& "if there is a suffix for the entry the index + the current bit + suffix + prefix equals total bit width");
	assert (SpatialSelectionOperationsUtil::lookup<DIM>(entry, initialNode, NULL).second == entry->id_);
	#endif

	// the root node might have changed
	return initialNode;
}

#endif /* SRC_UTIL_DYNAMICNODEOPERATIONSUTIL_H_ */
