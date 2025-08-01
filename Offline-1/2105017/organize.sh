# -------------------------- Function Definition --------------------------- #

usage() {
    echo "Usage:"
    echo "  $0 <submission_folder> <target_folder> <test_folder> <answer_folder> [optional flags]"
    echo
    echo "Mandatory Arguments:"
    echo "  submission_folder     Path to submissions directory (e.g., submissions)"
    echo "  target_folder         Path to target directory (e.g., targets)"
    echo "  test_folder           Path to test cases directory (e.g., tests)"
    echo "  answer_folder         Path to answer/output directory (e.g., answers)"
    echo
    echo "Optional Flags (in order):"
    echo "  -v         Verbose mode"
    echo "  -noexecute Skip execution and output comparison"
    echo "  -nolc      Skip line count"
    echo "  -nocc      Skip comment count"
    echo "  -nofc      Skip function count"
    echo
    echo "Example:"
    echo "  $0 submissions targets tests answers -v -noexecute -nolc -nocc -nofc"
    exit 1
}

print() {
    if [ "$VERBOSE" = true ]; then
        echo "$1"
    fi
}

findFile() {
    if [ -d "$1" ]; then
        for file in "$1"/*; do
            findFile "$file"
        done
    elif [ -f "$1" ]; then
        if [[ "$1" == *.c || "$1" == *.cpp || "$1" == *.java || "$1" == *.py ]]; then
            echo "$1"
        fi
    fi
}

count_lines() {
    wc -l < "$1"
}

count_comments() {
    ext=$2
    case "$ext" in
        C|C++|Java) grep -c "//" "$1" ;;
        Python) grep -c '#' "$1" ;;
        *) echo 0 ;;
    esac
}

count_functions() {
    ext=$2
    case "$ext" in
        C|C++) grep -cE '^\s*[a-zA-Z_][a-zA-Z0-9_<>]*\s+[a-zA-Z_][a-zA-Z0-9_]*\s*\(.*\)\s*' "$1" ;;
        Java) grep -cE '^\s*(public|private|protected)?\s*(static\s+)?[a-zA-Z_][a-zA-Z0-9_<>]*\s+[a-zA-Z_][a-zA-Z0-9_]*\s*\([^)]*\)\s*' "$1" ;;
        Python) grep -cE '^\s*def\s+[a-zA-Z_]\w*\s*\(.*\)\s*:' "$1" ;;
        *) echo 0 ;;
    esac
}

count() {
    local lineCount commentCount functionCount
    lineCount=$(count_lines "$1")
    commentCount=$(count_comments "$1" "$2")
    functionCount=$(count_functions "$1" "$2")

    echo "$lineCount:$commentCount:$functionCount"
}

# -------------------------- Argument Handling --------------------------- #

if [ "$#" -lt 4 ]; then
    usage
fi

SUBMISSION_DIR=$1
TARGET_DIR=$2
TEST_DIR=$3
ANSWER_DIR=$4

# echo "SUBMISSION_DIR: $SUBMISSION_DIR"
# echo "TARGET_DIR: $TARGET_DIR"
# echo "TEST_DIR: $TEST_DIR"
# echo "ANSWER_DIR: $ANSWER_DIR"
shift 4

VERBOSE=false
NOEXECUTE=false
NOLC=false
NOCC=false
NOFC=false

for arg in "$@"; do
    case "$arg" in
        -v) VERBOSE=true ;;
        -noexecute) NOEXECUTE=true ;;
        -nolc) NOLC=true ;;
        -nocc) NOCC=true ;;
        -nofc) NOFC=true ;;
    esac
done

# -------------------------- TASK - A --------------------------- #

mkdir -p $TARGET_DIR

touch "$TARGET_DIR/result.csv"
CSV="result.csv"
chmod 777 "$TARGET_DIR/$CSV"

header="student_id,student_name,language"
$NOEXECUTE || header+=",matched,not_matched"
$NOLC || header+=",line_count"
$NOCC || header+=",comment_count"
$NOFC || header+=",function_count"
echo "$header" > "$TARGET_DIR/$CSV"

cd $SUBMISSION_DIR || exit

for file in *.zip; do
    unzip -nq "$file"
done

cd ../$TARGET_DIR || exit

mkdir -p C C++ Java Python

cd ../$SUBMISSION_DIR || exit

for folder in */; do
    folderName=${folder: -8}
    studentId="${folder%/}"
    studentId="${studentId: -7}"

    print "Organizing files of $studentId"

    studentName="${folder%%_*}"
    file=$(findFile "$folder")
    # echo "$folder --> $(basename "$file")"
    if [[ "$file" = *.c ]]; then
        mkdir -p "../$TARGET_DIR/C/$folderName"
        cp "$file" "../$TARGET_DIR/C/$folderName/main.c"
    elif [[ "$file" = *.cpp ]]; then
        mkdir -p "../$TARGET_DIR/C++/$folderName"
        cp "$file" "../$TARGET_DIR/C++/$folderName/main.cpp"
    elif [[ "$file" = *.java ]]; then
        mkdir -p "../$TARGET_DIR/Java/$folderName"
        cp "$file" "../$TARGET_DIR/Java/$folderName/Main.java"
    elif [[ "$file" = *.py ]]; then
        mkdir -p "../$TARGET_DIR/Python/$folderName"
        cp "$file" "../$TARGET_DIR/Python/$folderName/main.py"
    fi

# -------------------------- TASK - B --------------------------- #

    language=

    if [[ "$file" = *.c ]]; then
        language="C"
    elif [[ "$file" = *.cpp ]]; then
        language="C++"
    elif [[ "$file" = *.java ]]; then
        language="Java"
    elif [[ "$file" = *.py ]]; then
        language="Python"
    fi

    
    counts=$(count "$file" "$language")

    lineCount="${counts%%:*}"
    rest="${counts#*:}"
    commentCount="${rest%%:*}"
    functionCount="${rest#*:}"

# -------------------------- TASK - C --------------------------- #

    matched=0
    not_matched=0

    if [ "$NOEXECUTE" = false ]; then
        print "Executing files of $studentId"
        cd ../$TARGET_DIR/$language/$studentId || exit

        case $language in
            C) gcc main.c -o main.out ;;
            C++) g++ main.cpp -o main.out ;;
            Java) javac Main.java ;;
        esac

        for testfile in ../../../$TEST_DIR/test*.txt; do
            base=$(basename $testfile)
            test=${base%.txt}
            testNum=${test: 4}

            outputFile="out${testNum}.txt"
            touch "$outputFile"
            answerFile="ans$testNum.txt"
            # echo "$testNum $outputFile $answerFile"

            case $language in
                C) ./main.out < "$testfile" > "$outputFile" ;;
                C++) ./main.out < "$testfile" > "$outputFile" ;;
                Java) java -cp . Main < "$testfile" > "$outputFile" ;;
                Python) python3 main.py < "$testfile" > "$outputFile" ;;
            esac

            if diff -qw "$outputFile" "../../../$ANSWER_DIR/$answerFile" > /dev/null; then
                matched=$((matched + 1))
            else
                not_matched=$((not_matched + 1))
            fi
            # echo "$matched $not_matched"
        done

        cd ../../../$SUBMISSION_DIR
    fi

    csvContent="$studentId,\"$studentName\",$language"
    $NOEXECUTE || csvContent+=",$matched,$not_matched"
    $NOLC || csvContent+=",$lineCount"
    $NOCC || csvContent+=",$commentCount"
    $NOFC || csvContent+=",$functionCount"
    echo "$csvContent" >> "../$TARGET_DIR/$CSV"


done

print "All submissions proceed successfully"