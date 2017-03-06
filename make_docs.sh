if [ ${PRIMARY} = "true" ] && [ ${TRAVIS_BRANCH} = "master" ] && [ ${TRAVIS_PULL_REQUEST} = "false" ]
then
    doxygen
    git config --global user.email "lodo1995-unitn@users.noreply.github.com"
    git config --global user.name "travis-ci"
    git clone --quiet --branch=gh-pages https://${GH_TOKEN}@github.com/lodo1995/experimental.xml.git gh-pages
    cd gh-pages
    git rm -rf .
    cp -r ../dist/html/* .
    git add -A
    git commit -m "Documentation updated by Travis CI (build $TRAVIS_BUILD_NUMBER)"
    git push https://${GH_TOKEN}@github.com/lodo1995/experimental.xml.git gh-pages
    cd ..
fi