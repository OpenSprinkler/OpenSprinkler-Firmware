#ifndef OTF_LINKEDMAP_H
#define OTF_LINKEDMAP_H

#define KEY_MAX_LENGTH 100

#include <utils.h>

namespace OTF {
  template<class T>
  class LinkedMapNode;

  template<class T>
  class LinkedMap {
    friend class OpenThingsFramework;

    friend class Response;

  private:
    LinkedMapNode<T> *head = nullptr;
    LinkedMapNode<T> *tail = nullptr;

    void _add(LinkedMapNode<T> *node) {
      if (head == nullptr) {
        head = node;
        tail = head;
      } else {
        tail->next = node;
        tail = tail->next;
      }
    }

    T _find(const char *key) const {
      LinkedMapNode<T> *node = head;
      while (node != nullptr) {
        if (strcmp(node->key, key) == 0) {
          return node->value;
        }

        node = node->next;
      }

      // Indicate the key could not be found.
      return nullptr;
    }

  public:
    ~LinkedMap() {
      LinkedMapNode<T> *node = head;
      while (node != nullptr) {
        LinkedMapNode<T> *next = node->next;
        delete node;
        node = next;
      }
    }

    void add(const char *key, T value) {
      _add(new LinkedMapNode<T>(key, value));
    }

    T find(const char *key) const {
      return _find(key);
    }
  };

  template<class T>
  class LinkedMapNode {
  private:
    /** Indicates if the key was copied into RAM from flash memory and needs to be freed when the object is destroyed. */
    //bool keyFromFlash = false;

  public:
    const char *key = nullptr;
    T value;
    LinkedMapNode<T> *next = nullptr;

    /*LinkedMapNode(const __FlashStringHelper *key, T value) {
      keyFromFlash = true;
      char *_key = new char[KEY_MAX_LENGTH];
      strncpy_P(_key, (char *) key, KEY_MAX_LENGTH);
      this->key = (const char *) _key;

      this->value = value;
    }*/

    LinkedMapNode(const char *key, T value) {
      this->key = key;
      this->value = value;
    }

    ~LinkedMapNode() {
      // Delete the key if it was copied into RAM from flash memory.
      //if (keyFromFlash) {
      //  delete key;
      //}
    }
  };
}// namespace OTF

#endif
