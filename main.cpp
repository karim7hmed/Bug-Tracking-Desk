// CS212 Data Structures - Course Project
// Topic 48: Bug Tracking Desk
//
// Stack       : recent status changes (undo)
// Queue       : bugs waiting for triage
// Linked list : developer assignments
// BST         : bug records, lookup by bug ID
// Merge sort  : severity report

#include <iostream>
#include <iomanip>
#include <string>
#include <cstdlib>
#include <cctype>

#include "Bug/Bug.h"
#include "Stack/Stack.h"
#include "Queue/Queue.h"
#include "LinkedList/LinkedList.h"
#include "BST/BST.h"
#include "Sorting/Sorting.h"

using namespace std;

// the four structures that make up the desk
BST bugs;                    // every bug record, key = bug ID
Queue triageQueue;           // IDs of new bugs waiting for triage
Stack history;               // status changes, used for undo
LinkedList assignments;      // which developer has which bug

const int MAX_TITLE = 40;
const int MAX_DEV = 14;

struct InputEnded {};        // thrown when stdin is closed

// ---------------------------------------------------------------
// input helpers
// ---------------------------------------------------------------

string trim(const string& s) {
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

string readLine(const string& prompt) {
    cout << prompt;
    string line;
    if (!getline(cin, line)) throw InputEnded();
    return trim(line);
}

int readInt(const string& prompt, int lo, int hi) {
    while (true) {
        string s = readLine(prompt);
        bool ok = !s.empty() && s.size() <= 9;
        size_t i = 0;
        if (ok && s[0] == '-') i = 1;
        if (i == s.size()) ok = false;
        for (; ok && i < s.size(); i++) {
            if (!isdigit((unsigned char)s[i])) ok = false;
        }
        if (ok) {
            int v = atoi(s.c_str());
            if (v >= lo && v <= hi) return v;
        }
        cout << "Invalid input. Enter a whole number from " << lo << " to " << hi << ".\n";
    }
}

string readText(const string& prompt, int maxLen, bool allowEmpty) {
    while (true) {
        string s = readLine(prompt);
        if (s.empty() && allowEmpty) return s;
        if (s.empty()) {
            cout << "This field cannot be empty.\n";
        } else if ((int)s.size() > maxLen) {
            cout << "Too long, the limit is " << maxLen << " characters.\n";
        } else {
            return s;
        }
    }
}

bool askYesNo(const string& prompt) {
    while (true) {
        string s = readLine(prompt);
        if (s == "y" || s == "Y") return true;
        if (s == "n" || s == "N") return false;
        cout << "Please answer y or n.\n";
    }
}

// ---------------------------------------------------------------
// printing
// ---------------------------------------------------------------

void printHeader() {
    cout << left << setw(8) << "ID" << setw(10) << "Severity" << setw(13) << "Status"
         << setw(16) << "Developer" << "Title\n";
    cout << string(72, '-') << "\n";
}

void printRow(const Bug& b) {
    Assignment* a = assignments.find(b.id);
    cout << left << setw(8) << b.id << setw(10) << severityName(b.severity)
         << setw(13) << statusName(b.status) << setw(16) << (a ? a->developer : "-")
         << b.title << "\n";
}

void printId(const Bug& b) {
    cout << b.id << " ";
}

void printMenu() {
    cout << "\n===== BUG TRACKING DESK =====\n";
    cout << " 1. Report a new bug\n";
    cout << " 2. Triage next bug (dequeue)\n";
    cout << " 3. Assign or reassign a developer\n";
    cout << " 4. Update bug status\n";
    cout << " 5. Undo last status change\n";
    cout << " 6. Search bug by ID\n";
    cout << " 7. Edit bug title or severity\n";
    cout << " 8. Delete a bug\n";
    cout << " 9. Show all bugs (inorder)\n";
    cout << "10. Show tree traversals\n";
    cout << "11. Show triage queue\n";
    cout << "12. Show status history (stack)\n";
    cout << "13. Show developer assignments\n";
    cout << "14. Severity report (merge sort)\n";
    cout << "15. Developer workload report\n";
    cout << "16. Trace a bug through all structures\n";
    cout << "17. Desk status (sizes)\n";
    cout << "18. Load sample data\n";
    cout << " 0. Exit\n";
}

// ---------------------------------------------------------------
// helpers shared by several menu options
// ---------------------------------------------------------------

// every status change goes through here so that the stack always matches the tree
void changeStatus(Bug& b, Status newStatus) {
    StatusChange c;
    c.bugId = b.id;
    c.oldStatus = b.status;
    c.newStatus = newStatus;
    history.push(c);
    b.status = newStatus;
}

// returns false and fills reason when the bug cannot be added
bool addBug(int id, const string& title, int severity, string& reason) {
    if (bugs.search(id) != nullptr) {
        reason = "A bug with ID " + to_string(id) + " already exists.";
        return false;
    }
    if (triageQueue.isFull()) {
        reason = "The triage queue is full (" + to_string(QUEUE_CAPACITY) +
                 "). Triage a bug first.";
        return false;
    }
    Bug b;
    b.id = id;
    b.title = title;
    b.severity = severity;
    b.status = NEW_BUG;
    bugs.insert(b);
    triageQueue.enqueue(id);
    return true;
}

bool canMove(Status from, Status to) {
    if (from == ASSIGNED && to == IN_PROGRESS) return true;
    if (from == IN_PROGRESS && to == RESOLVED) return true;
    if (from == RESOLVED && to == CLOSED) return true;
    if (from == RESOLVED && to == IN_PROGRESS) return true;   // reopen
    return false;
}

// ---------------------------------------------------------------
// menu options
// ---------------------------------------------------------------

void reportBug() {
    int id = readInt("Bug ID (1-999999): ", 1, 999999);
    if (bugs.search(id) != nullptr) {
        cout << "A bug with ID " << id << " already exists.\n";
        return;
    }
    string title = readText("Title (max 40 chars): ", MAX_TITLE, false);
    int sev = readInt("Severity (1=Low 2=Medium 3=High 4=Critical): ", 1, 4);

    string reason;
    if (addBug(id, title, sev, reason)) {
        cout << "Bug " << id << " saved and added to the triage queue (position "
             << triageQueue.positionOf(id) << ").\n";
    } else {
        cout << reason << "\n";
    }
}

void assignFirst(Bug& b, const string& dev) {
    assignments.append(b.id, dev);
    changeStatus(b, ASSIGNED);
    cout << "Bug " << b.id << " assigned to " << dev << ".\n";
}

void triageNext() {
    if (triageQueue.isEmpty()) {
        cout << "The triage queue is empty, nothing to triage.\n";
        return;
    }
    int id;
    triageQueue.front(id);
    Bug* b = bugs.search(id);
    if (b == nullptr) {
        triageQueue.dequeue(id);
        cout << "Bug " << id << " was in the queue but not in the tree. Entry removed.\n";
        return;
    }
    cout << "Next bug in the queue:\n";
    printHeader();
    printRow(*b);

    int sev = readInt("Severity after triage (1-4, 0 keeps " + to_string(b->severity) + "): ", 0, 4);
    triageQueue.dequeue(id);
    if (sev != 0) b->severity = sev;
    changeStatus(*b, TRIAGED);
    cout << "Bug " << id << " triaged.\n";

    string dev = readText("Developer to assign now (blank to skip): ", MAX_DEV, true);
    if (!dev.empty()) assignFirst(*b, dev);
}

void assignDeveloper() {
    int id = readInt("Bug ID: ", 1, 999999);
    Bug* b = bugs.search(id);
    if (b == nullptr) {
        cout << "Bug " << id << " not found.\n";
        return;
    }
    if (b->status == NEW_BUG) {
        cout << "Bug " << id << " is still in the triage queue. Triage it first.\n";
        return;
    }
    if (b->status == RESOLVED || b->status == CLOSED) {
        cout << "Bug " << id << " is " << statusName(b->status) << ", it cannot be assigned.\n";
        return;
    }
    string dev = readText("Developer name (max 14 chars): ", MAX_DEV, false);
    if (b->status == TRIAGED) {
        assignFirst(*b, dev);
    } else {
        assignments.reassign(id, dev);
        cout << "Bug " << id << " reassigned to " << dev << ".\n";
    }
}

void updateStatus() {
    int id = readInt("Bug ID: ", 1, 999999);
    Bug* b = bugs.search(id);
    if (b == nullptr) {
        cout << "Bug " << id << " not found.\n";
        return;
    }
    cout << "Current status: " << statusName(b->status) << "\n";
    if (b->status == NEW_BUG) {
        cout << "This bug is waiting in the triage queue. Use option 2.\n";
        return;
    }
    if (b->status == TRIAGED) {
        cout << "This bug has no developer yet. Use option 3.\n";
        return;
    }
    if (b->status == CLOSED) {
        cout << "Closed bugs cannot be changed.\n";
        return;
    }
    int choice = readInt("New status (3=In Progress 4=Resolved 5=Closed): ", 3, 5);
    Status target = (Status)choice;
    if (!canMove(b->status, target)) {
        cout << "Not allowed: " << statusName(b->status) << " -> " << statusName(target) << ".\n";
        return;
    }
    changeStatus(*b, target);
    cout << "Bug " << id << " is now " << statusName(target) << ".\n";
}

void undoLast() {
    StatusChange c;
    if (!history.peek(c)) {
        cout << "Nothing to undo, the history is empty.\n";
        return;
    }
    Bug* b = bugs.search(c.bugId);
    if (b == nullptr) {
        history.pop(c);
        cout << "History entry for missing bug " << c.bugId << " removed.\n";
        return;
    }
    // undoing a triage puts the bug back in the queue, so there must be room
    if (c.newStatus == TRIAGED && triageQueue.isFull()) {
        cout << "Cannot undo: the triage queue is full.\n";
        return;
    }
    history.pop(c);
    if (c.newStatus == TRIAGED) {
        triageQueue.enqueue(c.bugId);
        cout << "Bug " << c.bugId << " put back at the end of the triage queue.\n";
    } else if (c.newStatus == ASSIGNED) {
        assignments.removeByBug(c.bugId);
        cout << "Assignment of bug " << c.bugId << " removed from the list.\n";
    }
    b->status = c.oldStatus;
    cout << "Undone: bug " << c.bugId << " is back to " << statusName(c.oldStatus) << ".\n";
}

void searchBug() {
    int id = readInt("Bug ID: ", 1, 999999);
    int steps;
    Bug* b = bugs.search(id, steps);
    if (b == nullptr) {
        cout << "Bug " << id << " not found (" << steps << " comparisons).\n";
        return;
    }
    printHeader();
    printRow(*b);
    cout << "Found after " << steps << " comparison(s).\n";
}

void editBug() {
    int id = readInt("Bug ID: ", 1, 999999);
    Bug* b = bugs.search(id);
    if (b == nullptr) {
        cout << "Bug " << id << " not found.\n";
        return;
    }
    int what = readInt("Edit 1=title 2=severity: ", 1, 2);
    if (what == 1) {
        b->title = readText("New title (max 40 chars): ", MAX_TITLE, false);
    } else {
        b->severity = readInt("New severity (1-4): ", 1, 4);
    }
    cout << "Bug updated.\n";
    printHeader();
    printRow(*b);
}

void deleteBug() {
    int id = readInt("Bug ID: ", 1, 999999);
    Bug* b = bugs.search(id);
    if (b == nullptr) {
        cout << "Bug " << id << " not found.\n";
        return;
    }
    printHeader();
    printRow(*b);
    if (!askYesNo("Delete this bug? (y/n): ")) {
        cout << "Delete cancelled.\n";
        return;
    }
    bool inList = assignments.removeByBug(id);
    bool inQueue = triageQueue.removeById(id);
    int histRemoved = history.removeByBug(id);
    bugs.remove(id);

    cout << "Bug " << id << " deleted.\n";
    cout << "  removed from tree: yes\n";
    cout << "  removed from triage queue: " << (inQueue ? "yes" : "was not queued") << "\n";
    cout << "  removed from assignment list: " << (inList ? "yes" : "was not assigned") << "\n";
    cout << "  history entries removed: " << histRemoved << "\n";
}

void showAll() {
    if (bugs.isEmpty()) {
        cout << "There are no bugs yet.\n";
        return;
    }
    printHeader();
    bugs.inorder(printRow);
    cout << bugs.size() << " bug(s) in total.\n";
}

void showTraversals() {
    if (bugs.isEmpty()) {
        cout << "The tree is empty.\n";
        return;
    }
    cout << "Inorder   : ";
    bugs.inorder(printId);
    cout << "\nPreorder  : ";
    bugs.preorder(printId);
    cout << "\nPostorder : ";
    bugs.postorder(printId);
    cout << "\nNodes: " << bugs.size() << ", height: " << bugs.height() << "\n";
}

void severityReport() {
    int n = bugs.size();
    if (n == 0) {
        cout << "There are no bugs yet.\n";
        return;
    }
    Bug** arr = new Bug*[n];
    bugs.toArray(arr);            // inorder, so already sorted by ID
    Sorting::mergeSortBySeverity(arr, n);
    cout << "Bugs sorted by severity (highest first, ties by ID):\n";
    printHeader();
    for (int i = 0; i < n; i++) printRow(*arr[i]);
    delete[] arr;
}

void traceBug() {
    int id = readInt("Bug ID: ", 1, 999999);
    int steps;
    Bug* b = bugs.search(id, steps);
    if (b == nullptr) {
        cout << "Bug " << id << " is not in the tree, so it is not tracked anywhere.\n";
        return;
    }
    cout << "Tree      : found in " << steps << " step(s), status " << statusName(b->status)
         << ", severity " << severityName(b->severity) << "\n";
    int pos = triageQueue.positionOf(id);
    if (pos > 0) cout << "Queue     : waiting for triage, position " << pos << "\n";
    else cout << "Queue     : not in the queue\n";
    Assignment* a = assignments.find(id);
    if (a != nullptr) cout << "List      : assigned to " << a->developer << "\n";
    else cout << "List      : no developer assigned\n";
    cout << "Stack     : " << history.countForBug(id) << " status change(s) in the history\n";
}

void showStatus() {
    cout << "Bugs in tree      : " << bugs.size() << " (height " << bugs.height() << ")"
         << (bugs.isEmpty() ? "  [empty]" : "") << "\n";
    cout << "Triage queue      : " << triageQueue.size() << " of " << QUEUE_CAPACITY
         << (triageQueue.isEmpty() ? "  [empty]" : "")
         << (triageQueue.isFull() ? "  [full]" : "") << "\n";
    cout << "Status history    : " << history.size()
         << (history.isEmpty() ? "  [empty]" : "") << "\n";
    cout << "Assignments       : " << assignments.size()
         << (assignments.isEmpty() ? "  [empty]" : "") << "\n";
}

void loadSample() {
    struct Sample { int id; const char* title; int sev; };
    Sample data[] = {
        {105, "Login page crashes on empty password", 4},
        {101, "Typo in settings menu", 1},
        {110, "Report export is very slow", 2},
        {103, "Profile image is not saved", 3},
        {120, "Payment fails for large orders", 4},
        {108, "Dark mode text is unreadable", 2}
    };
    int added = 0;
    for (int i = 0; i < 6; i++) {
        string reason;
        if (addBug(data[i].id, data[i].title, data[i].sev, reason)) {
            added++;
        } else {
            cout << "Bug " << data[i].id << " skipped: " << reason << "\n";
        }
    }
    cout << added << " sample bug(s) loaded.\n";
}

// ---------------------------------------------------------------

int main() {
    try {
        while (true) {
            printMenu();
            int choice = readInt("Enter choice: ", 0, 18);
            cout << "\n";
            if (choice == 0) {
                cout << "Goodbye.\n";
                break;
            }
            switch (choice) {
                case 1:  reportBug(); break;
                case 2:  triageNext(); break;
                case 3:  assignDeveloper(); break;
                case 4:  updateStatus(); break;
                case 5:  undoLast(); break;
                case 6:  searchBug(); break;
                case 7:  editBug(); break;
                case 8:  deleteBug(); break;
                case 9:  showAll(); break;
                case 10: showTraversals(); break;
                case 11: triageQueue.display(); break;
                case 12: history.display(); break;
                case 13: assignments.display(); break;
                case 14: severityReport(); break;
                case 15: assignments.printWorkload(); break;
                case 16: traceBug(); break;
                case 17: showStatus(); break;
                case 18: loadSample(); break;
            }
        }
    } catch (InputEnded&) {
        cout << "\nInput ended, closing the program.\n";
    }
    return 0;
}
