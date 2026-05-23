#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

const string DATA_FILE = "bank-data-cpp.txt";

tm getLocalTime(time_t current) {
    tm localTime{};
    tm* timePointer = localtime(&current);
    if (timePointer != nullptr) {
        localTime = *timePointer;
    }
    return localTime;
}

string nowText() {
    auto now = chrono::system_clock::now();
    time_t current = chrono::system_clock::to_time_t(now);
    tm localTime = getLocalTime(current);
    ostringstream out;
    out << put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

string dateAfterMonths(int months) {
    time_t current = time(nullptr);
    tm localTime = getLocalTime(current);
    localTime.tm_mon += months;
    mktime(&localTime);
    ostringstream out;
    out << put_time(&localTime, "%Y-%m-%d");
    return out.str();
}

string encode(const string& value) {
    string result;
    for (char ch : value) {
        if (ch == '%') result += "%25";
        else if (ch == '|') result += "%7C";
        else result += ch;
    }
    return result;
}

string decode(const string& value) {
    string result;
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            string code = value.substr(i, 3);
            if (code == "%25") {
                result += '%';
                i += 2;
            } else if (code == "%7C") {
                result += '|';
                i += 2;
            } else {
                result += value[i];
            }
        } else {
            result += value[i];
        }
    }
    return result;
}

vector<string> split(const string& text, char delimiter) {
    vector<string> parts;
    string item;
    stringstream ss(text);
    while (getline(ss, item, delimiter)) {
        parts.push_back(item);
    }
    return parts;
}

long long parseMoney(const string& input) {
    string value;
    for (char ch : input) {
        if (ch != ',' && !isspace(static_cast<unsigned char>(ch))) {
            value += ch;
        }
    }
    if (value.empty()) {
        throw invalid_argument("Amount cannot be empty.");
    }
    bool negative = false;
    if (value[0] == '-') {
        negative = true;
        value.erase(value.begin());
    }
    size_t dot = value.find('.');
    string rupees = dot == string::npos ? value : value.substr(0, dot);
    string paise = dot == string::npos ? "" : value.substr(dot + 1);
    if (rupees.empty()) rupees = "0";
    if (paise.size() == 0) paise = "00";
    if (paise.size() == 1) paise += "0";
    if (paise.size() > 2) paise = paise.substr(0, 2);
    long long result = stoll(rupees) * 100 + stoll(paise);
    return negative ? -result : result;
}

string formatMoney(long long paise) {
    bool negative = paise < 0;
    paise = llabs(paise);
    ostringstream out;
    if (negative) out << "-";
    out << (paise / 100) << "." << setw(2) << setfill('0') << (paise % 100);
    return out.str();
}

struct Transaction {
    string timestamp;
    string type;
    long long amount;
    long long balanceAfter;
    string note;

    void display() const {
        cout << timestamp << " | " << setw(10) << left << type << " | "
             << setw(12) << right << formatMoney(amount) << " | Balance "
             << setw(12) << formatMoney(balanceAfter) << " | " << note << "\n";
    }
};

struct FixedDeposit {
    long long principal;
    double annualRate;
    int months;
    string openedOn;
    string maturityDate;
    long long maturityAmount;

    void display() const {
        cout << "  FD " << formatMoney(principal) << " at " << annualRate << "% for "
             << months << " months, matures " << maturityDate << " for "
             << formatMoney(maturityAmount) << "\n";
    }
};

struct Loan {
    long long principal;
    double annualRate;
    int tenureMonths;
    long long emi;
    long long outstanding;
    int paidInstallments;

    void display() const {
        cout << "  Loan " << formatMoney(principal) << " at " << annualRate
             << "% | EMI " << formatMoney(emi) << " | Outstanding "
             << formatMoney(outstanding) << " | Paid " << paidInstallments
             << "/" << tenureMonths << "\n";
    }
};

class Account {
public:
    long long accountNumber;
    string holderName;
    string type;
    long long balance;
    double annualInterestRate;
    long long maintenanceFee;
    vector<Transaction> transactions;
    vector<FixedDeposit> fixedDeposits;
    vector<Loan> loans;

    Account() = default;

    Account(long long number, string holder, string accountType, long long openingBalance,
            double rate, long long fee)
        : accountNumber(number), holderName(move(holder)), type(move(accountType)),
          balance(openingBalance), annualInterestRate(rate), maintenanceFee(fee) {
        addTransaction("OPEN", openingBalance, "Account opened");
    }

    void deposit(long long amount, const string& note) {
        requirePositive(amount);
        balance += amount;
        addTransaction("CREDIT", amount, note);
    }

    void withdraw(long long amount, const string& note) {
        requirePositive(amount);
        if (amount > balance) {
            throw runtime_error("Insufficient balance.");
        }
        balance -= amount;
        addTransaction("DEBIT", amount, note);
    }

    void applyInterestOrFee() {
        if (type == "SAVING") {
            long long interest = llround(balance * annualInterestRate / 100.0);
            balance += interest;
            addTransaction("INTEREST", interest, "Yearly savings interest");
        } else {
            withdraw(maintenanceFee, "Current account maintenance fee");
        }
    }

    void openFixedDeposit(long long amount, double annualRate, int months) {
        requirePositive(amount);
        if (months <= 0) {
            throw runtime_error("FD duration must be positive.");
        }
        withdraw(amount, "Fixed deposit opened");
        long long maturityAmount = amount + llround(amount * annualRate * months / 1200.0);
        fixedDeposits.push_back({amount, annualRate, months, nowText().substr(0, 10), dateAfterMonths(months), maturityAmount});
    }

    void createLoan(long long principal, double annualRate, int months) {
        requirePositive(principal);
        if (months <= 0) {
            throw runtime_error("Loan tenure must be positive.");
        }
        long long emi = calculateEmi(principal, annualRate, months);
        loans.push_back({principal, annualRate, months, emi, emi * months, 0});
        deposit(principal, "Loan disbursed");
    }

    void payEmi() {
        if (loans.empty()) {
            throw runtime_error("No loan exists for this account.");
        }
        Loan& loan = loans.back();
        if (loan.outstanding <= 0) {
            throw runtime_error("Loan is already closed.");
        }
        long long amount = min(loan.emi, loan.outstanding);
        withdraw(amount, "EMI payment");
        loan.outstanding -= amount;
        loan.paidInstallments++;
    }

    void printDetails() const {
        cout << "\nAccount number : " << accountNumber << "\n";
        cout << "Holder name    : " << holderName << "\n";
        cout << "Type           : " << type << "\n";
        cout << "Balance        : " << formatMoney(balance) << "\n";
        cout << "Fixed deposits : " << fixedDeposits.size() << "\n";
        for (const auto& fd : fixedDeposits) fd.display();
        cout << "Loans          : " << loans.size() << "\n";
        for (const auto& loan : loans) loan.display();
    }

    static long long calculateEmi(long long principal, double annualRate, int months) {
        double monthlyRate = annualRate / 1200.0;
        if (monthlyRate == 0.0) {
            return llround(static_cast<double>(principal) / months);
        }
        double power = pow(1.0 + monthlyRate, months);
        double emi = principal * monthlyRate * power / (power - 1.0);
        return llround(emi);
    }

private:
    void addTransaction(const string& txType, long long amount, const string& note) {
        transactions.push_back({nowText(), txType, amount, balance, note});
    }

    static void requirePositive(long long amount) {
        if (amount <= 0) {
            throw runtime_error("Amount must be greater than zero.");
        }
    }
};

class Bank {
private:
    map<long long, Account> accounts;
    long long nextAccountNumber = 100001;

public:
    void load() {
        ifstream file(DATA_FILE);
        if (!file) return;

        string line;
        while (getline(file, line)) {
            vector<string> p = split(line, '|');
            if (p.empty()) continue;
            if (p[0] == "NEXT" && p.size() >= 2) {
                nextAccountNumber = stoll(p[1]);
            } else if (p[0] == "ACCOUNT" && p.size() >= 8) {
                Account account;
                account.accountNumber = stoll(p[1]);
                account.holderName = decode(p[2]);
                account.type = p[3];
                account.balance = stoll(p[4]);
                account.annualInterestRate = stod(p[5]);
                account.maintenanceFee = stoll(p[6]);
                accounts[account.accountNumber] = account;
            } else if (p[0] == "TXN" && p.size() >= 7) {
                long long number = stoll(p[1]);
                accounts[number].transactions.push_back({decode(p[2]), p[3], stoll(p[4]), stoll(p[5]), decode(p[6])});
            } else if (p[0] == "FD" && p.size() >= 8) {
                long long number = stoll(p[1]);
                accounts[number].fixedDeposits.push_back({stoll(p[2]), stod(p[3]), stoi(p[4]), p[5], p[6], stoll(p[7])});
            } else if (p[0] == "LOAN" && p.size() >= 8) {
                long long number = stoll(p[1]);
                accounts[number].loans.push_back({stoll(p[2]), stod(p[3]), stoi(p[4]), stoll(p[5]), stoll(p[6]), stoi(p[7])});
            }
        }
    }

    void save() const {
        ofstream file(DATA_FILE);
        file << "NEXT|" << nextAccountNumber << "\n";
        for (const auto& pair : accounts) {
            const Account& a = pair.second;
            file << "ACCOUNT|" << a.accountNumber << "|" << encode(a.holderName) << "|"
                 << a.type << "|" << a.balance << "|" << a.annualInterestRate << "|"
                 << a.maintenanceFee << "\n";
            for (const auto& tx : a.transactions) {
                file << "TXN|" << a.accountNumber << "|" << encode(tx.timestamp) << "|"
                     << tx.type << "|" << tx.amount << "|" << tx.balanceAfter << "|"
                     << encode(tx.note) << "\n";
            }
            for (const auto& fd : a.fixedDeposits) {
                file << "FD|" << a.accountNumber << "|" << fd.principal << "|"
                     << fd.annualRate << "|" << fd.months << "|" << fd.openedOn
                     << "|" << fd.maturityDate << "|" << fd.maturityAmount << "\n";
            }
            for (const auto& loan : a.loans) {
                file << "LOAN|" << a.accountNumber << "|" << loan.principal << "|"
                     << loan.annualRate << "|" << loan.tenureMonths << "|" << loan.emi
                     << "|" << loan.outstanding << "|" << loan.paidInstallments << "\n";
            }
        }
    }

    Account& createAccount(const string& holder, const string& type, long long openingBalance,
                           double interestRate, long long maintenanceFee) {
        string accountType = uppercase(type);
        if (accountType != "SAVING" && accountType != "CURRENT") {
            throw runtime_error("Account type must be saving or current.");
        }
        long long number = nextAccountNumber++;
        accounts[number] = Account(number, holder, accountType, openingBalance,
                                   accountType == "SAVING" ? interestRate : 0.0,
                                   accountType == "CURRENT" ? maintenanceFee : 0);
        return accounts[number];
    }

    Account& find(long long accountNumber) {
        auto it = accounts.find(accountNumber);
        if (it == accounts.end()) {
            throw runtime_error("Account not found.");
        }
        return it->second;
    }

    vector<Account*> findByName(const string& name) {
        vector<Account*> matches;
        string search = lowercase(name);
        for (auto& pair : accounts) {
            if (lowercase(pair.second.holderName).find(search) != string::npos) {
                matches.push_back(&pair.second);
            }
        }
        return matches;
    }

    void listAccounts() const {
        if (accounts.empty()) {
            cout << "No accounts found.\n";
            return;
        }
        for (const auto& pair : accounts) {
            const Account& a = pair.second;
            cout << a.accountNumber << " | " << setw(22) << left << a.holderName
                 << " | " << setw(7) << a.type << " | Balance " << formatMoney(a.balance) << "\n";
        }
    }

private:
    static string lowercase(string value) {
        transform(value.begin(), value.end(), value.begin(),
                  [](unsigned char ch) { return static_cast<char>(tolower(ch)); });
        return value;
    }

    static string uppercase(string value) {
        transform(value.begin(), value.end(), value.begin(),
                  [](unsigned char ch) { return static_cast<char>(toupper(ch)); });
        return value;
    }
};

string readLine(const string& prompt) {
    cout << prompt;
    string value;
    getline(cin, value);
    return value;
}

int readInt(const string& prompt) {
    while (true) {
        try {
            return stoi(readLine(prompt));
        } catch (...) {
            cout << "Enter a valid whole number.\n";
        }
    }
}

long long readLongLong(const string& prompt) {
    while (true) {
        try {
            return stoll(readLine(prompt));
        } catch (...) {
            cout << "Enter a valid number.\n";
        }
    }
}

double readRate(const string& prompt) {
    while (true) {
        try {
            double value = stod(readLine(prompt));
            if (value < 0) {
                cout << "Rate cannot be negative.\n";
                continue;
            }
            return value;
        } catch (...) {
            cout << "Enter a valid rate.\n";
        }
    }
}

long long readMoney(const string& prompt) {
    while (true) {
        try {
            long long amount = parseMoney(readLine(prompt));
            if (amount < 0) {
                cout << "Amount cannot be negative.\n";
                continue;
            }
            return amount;
        } catch (...) {
            cout << "Enter a valid amount.\n";
        }
    }
}

void printMenu() {
    cout << "\n==== Banking Management System ====\n";
    cout << "1. Create account\n";
    cout << "2. Deposit\n";
    cout << "3. Withdraw\n";
    cout << "4. Transfer\n";
    cout << "5. Apply yearly interest / current fee\n";
    cout << "6. Open fixed deposit\n";
    cout << "7. Create EMI loan\n";
    cout << "8. Pay EMI\n";
    cout << "9. Account details\n";
    cout << "10. Transaction history\n";
    cout << "11. List all accounts\n";
    cout << "12. Find account number by holder name\n";
    cout << "13. Save and exit\n";
}

void printNewAccountSlip(const Account& account) {
    cout << "\n========================================\n";
    cout << "ACCOUNT CREATED SUCCESSFULLY\n";
    cout << "Your account number is: " << account.accountNumber << "\n";
    cout << "Holder name           : " << account.holderName << "\n";
    cout << "Account type          : " << account.type << "\n";
    cout << "Opening balance       : " << formatMoney(account.balance) << "\n";
    cout << "Please remember this account number for deposit, withdraw, FD, EMI, and history.\n";
    cout << "========================================\n";
}

int main() {
    Bank bank;
    bank.load();

    while (true) {
        printMenu();
        int choice = readInt("Choose an option: ");
        try {
            if (choice == 1) {
                string holder = readLine("Account holder name: ");
                string type = readLine("Type (saving/current): ");
                long long openingBalance = readMoney("Initial deposit: ");
                double rate = 0.0;
                long long fee = 0;
                if (type == "saving" || type == "SAVING") {
                    rate = readRate("Annual interest rate (%): ");
                } else {
                    fee = readMoney("Monthly maintenance fee: ");
                }
                Account& account = bank.createAccount(holder, type, openingBalance, rate, fee);
                printNewAccountSlip(account);
            } else if (choice == 2) {
                Account& account = bank.find(readLongLong("Account number: "));
                account.deposit(readMoney("Deposit amount: "), "Cash deposit");
                cout << "Deposited. Balance: " << formatMoney(account.balance) << "\n";
            } else if (choice == 3) {
                Account& account = bank.find(readLongLong("Account number: "));
                account.withdraw(readMoney("Withdraw amount: "), "Cash withdrawal");
                cout << "Withdrawn. Balance: " << formatMoney(account.balance) << "\n";
            } else if (choice == 4) {
                Account& source = bank.find(readLongLong("From account number: "));
                Account& target = bank.find(readLongLong("To account number: "));
                long long amount = readMoney("Transfer amount: ");
                source.withdraw(amount, "Transfer to " + to_string(target.accountNumber));
                target.deposit(amount, "Transfer from " + to_string(source.accountNumber));
                cout << "Transfer complete.\n";
            } else if (choice == 5) {
                Account& account = bank.find(readLongLong("Account number: "));
                account.applyInterestOrFee();
                cout << "Updated balance: " << formatMoney(account.balance) << "\n";
            } else if (choice == 6) {
                Account& account = bank.find(readLongLong("Account number: "));
                long long amount = readMoney("FD amount: ");
                double rate = readRate("FD annual rate (%): ");
                int months = readInt("FD duration in months: ");
                account.openFixedDeposit(amount, rate, months);
                cout << "FD opened. Balance: " << formatMoney(account.balance) << "\n";
            } else if (choice == 7) {
                Account& account = bank.find(readLongLong("Account number: "));
                long long principal = readMoney("Loan principal: ");
                double rate = readRate("Annual interest rate (%): ");
                int months = readInt("Tenure in months: ");
                account.createLoan(principal, rate, months);
                cout << "Loan created and amount credited. EMI: "
                     << formatMoney(account.loans.back().emi) << "\n";
            } else if (choice == 8) {
                Account& account = bank.find(readLongLong("Account number: "));
                account.payEmi();
                cout << "EMI paid. Balance: " << formatMoney(account.balance) << "\n";
                cout << "Remaining loan amount: " << formatMoney(account.loans.back().outstanding) << "\n";
            } else if (choice == 9) {
                bank.find(readLongLong("Account number: ")).printDetails();
            } else if (choice == 10) {
                Account& account = bank.find(readLongLong("Account number: "));
                if (account.transactions.empty()) {
                    cout << "No transactions yet.\n";
                }
                for (const auto& tx : account.transactions) tx.display();
            } else if (choice == 11) {
                bank.listAccounts();
            } else if (choice == 12) {
                string name = readLine("Enter holder name or part of name: ");
                vector<Account*> matches = bank.findByName(name);
                if (matches.empty()) {
                    cout << "No account found for this name.\n";
                } else {
                    cout << "\nMatching accounts:\n";
                    for (const Account* account : matches) {
                        cout << "Account No: " << account->accountNumber
                             << " | Name: " << account->holderName
                             << " | Type: " << account->type
                             << " | Balance: " << formatMoney(account->balance) << "\n";
                    }
                }
            } else if (choice == 13) {
                bank.save();
                cout << "Data saved. Goodbye.\n";
                break;
            } else {
                cout << "Invalid option.\n";
            }
        } catch (const exception& ex) {
            cout << "Error: " << ex.what() << "\n";
        }
        bank.save();
    }

    return 0;
}
