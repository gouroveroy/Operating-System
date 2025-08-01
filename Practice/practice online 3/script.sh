while IFS= read -r file; do
    pages=$(pdfinfo "$file" | grep pages | awk '{print $2}')
    echo "$pages"
done < <(find . -type f -name "*.pdf")